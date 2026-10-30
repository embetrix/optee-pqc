#!/usr/bin/env python3
#
# (C) Copyright 2026
# Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
#
"""Build an OP-TEE adaptation beside an untouched upstream Meson archive."""
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SHIM = ROOT / 'ta/leancrypto'
UPSTREAM = ROOT / 'external/leancrypto'
RNG_SYMBOLS = {'lc_seeded_rng', 'lc_seeded_rng_internal',
               'lc_seeded_rng_zero_state', 'lc_seeded_rng_status'}


def output_directory(value):
    if not value or not re.fullmatch(r'[a-zA-Z0-9_./+\-]+', value):
        raise ValueError('O must be a build path without spaces or shell metacharacters')
    output = Path(value).resolve()
    for source in (ROOT / 'ta', UPSTREAM):
        if output == source or output in source.parents or source in output.parents:
            raise ValueError('O must be separate from the TA and leancrypto sources, '
                             'not inside or above them')
    # A preexisting symlink must not redirect clean/build into another directory.
    if (output / 'leancrypto').resolve() != output / 'leancrypto':
        raise ValueError('The leancrypto output directory must not be a symlink')
    return output


def run(*args, capture=False):
    return subprocess.run([str(arg) for arg in args], check=True, text=True,
                          stdout=subprocess.PIPE if capture else None).stdout


def write_changed(path, text):
    if path.exists() and path.read_text() == text:
        return
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(text)
    temporary.replace(path)


def feature_header(database):
    """Use a common source's flags, excluding algorithm-specific TU defines."""
    entries = json.loads(database.read_text())
    entry = next((item for item in entries
                  if Path(item['file']).name == 'uuid.c'), None)
    if entry is None:
        raise ValueError('Upstream compilation database has no uuid.c entry')
    flags = entry.get('arguments') or shlex.split(entry['command'])
    defines = {}
    for flag in flags:
        match = re.fullmatch(r'-D(LC_[A-Za-z0-9_]+)(?:=(.*))?', flag)
        if match:
            defines[match[1]] = match[2] or ''
    required = {'LC_STATIC', 'LC_MEM_ON_HEAP', 'LC_SELFTEST_ENABLED',
                'LC_KYBER', 'LC_DILITHIUM', 'LC_DRNG_PRESENT'}
    if not required <= defines.keys():
        raise ValueError('Missing required leancrypto features: ' +
                         ', '.join(sorted(required - defines.keys())))
    lines = ['/* Generated from Meson compile_commands.json; do not edit. */',
             '#ifndef PQC_LEANCRYPTO_FEATURES_H', '#define PQC_LEANCRYPTO_FEATURES_H']
    lines += [f'#define {name}' + (f' {value}' if value else '')
              for name, value in sorted(defines.items())]
    return '\n'.join(lines + ['#endif', ''])


def defined_rng_symbols(nm, archive):
    symbols = run(nm, '--defined-only', '-g', archive, capture=True)
    return {line.split()[-1] for line in symbols.splitlines() if line.split()} & RNG_SYMBOLS


def adapt_archive(prefix, upstream, destination):
    """Keep Meson's output intact; the TA supplies the RNG implementations."""
    ar, nm = prefix + 'gcc-ar', prefix + 'gcc-nm'
    members = run(ar, 't', upstream, capture=True).splitlines()
    member = 'drng_src_seeded_rng.c.o'
    if members.count(member) != 1 or defined_rng_symbols(nm, upstream) != RNG_SYMBOLS:
        raise ValueError('Upstream seeded RNG layout changed; review the OP-TEE adapter')
    temporary = destination.with_suffix('.a.tmp')
    shutil.copyfile(upstream, temporary)
    try:
        run(ar, 'd', temporary, member)
        if defined_rng_symbols(nm, temporary):
            raise ValueError('Adapted archive still defines seeded RNG symbols')
        if destination.exists() and destination.read_bytes() == temporary.read_bytes():
            temporary.unlink()
        else:
            temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)


def build():
    output = Path(os.environ['LC_BUILD']).resolve()
    if output != output_directory(str(output.parent)) / 'leancrypto':
        raise ValueError('LC_BUILD must be O/leancrypto')
    devkit = Path(os.environ['TA_DEV_KIT_DIR']).resolve()
    arch = os.environ['LC_ARCH']
    families = {'ta_arm32': 'arm', 'ta_arm64': 'aarch64'}
    if arch not in families:
        raise ValueError(f'Unsupported TA architecture: {arch}')
    prefix = os.environ['LC_COMPILER_PREFIX']
    if not prefix:
        raise ValueError('CROSS_COMPILE must select the TA compiler')
    compiler = shutil.which(prefix + 'gcc')
    if compiler is None:
        raise ValueError(f'TA compiler not found: {prefix}gcc')
    gcc_headers = run(compiler, '-print-file-name=include', capture=True).strip()
    # These are the devkit's resolved flags, supplied by ta/Makefile.
    flags = shlex.split(os.environ['LC_CPPFLAGS']) + shlex.split(os.environ['LC_CFLAGS'])
    flags += ['-nostdinc', '-I' + gcc_headers, '-I' + str(devkit / 'include'),
              '-I' + str(UPSTREAM / 'internal/api'), '-I' + str(SHIM),
              '-I' + str(SHIM / 'compat'), '-fstack-usage',
              '-include', str(SHIM / 'lc_optee_config.h')]
    # Meson's machine files accept single-quoted strings with escaped slashes/quotes.
    def quote(value):
        return "'" + value.replace('\\', '\\\\').replace("'", "\\'") + "'"
    cross = '\n'.join([
        '# Generated OP-TEE TA environment.', '[binaries]',
        'c = ' + quote(compiler), 'ar = ' + quote(prefix + 'gcc-ar'),
        'strip = ' + quote(prefix + 'strip'), '[host_machine]', "system = 'none'",
        'cpu_family = ' + quote(families[arch]), 'cpu = ' + quote(families[arch]),
        "endian = 'little'", '[properties]', 'skip_sanity_check = true',
        '[built-in options]', 'c_args = [' + ', '.join(map(quote, flags)) + ']', ''])
    options = [line.split('#', 1)[0].strip()
               for line in (SHIM / 'meson-options.txt').read_text().splitlines()]
    options = [line for line in options if line]
    identity = hashlib.sha256((cross + run(compiler, '--version', capture=True) +
                               (devkit / 'mk/conf.mk').read_text()).encode()).hexdigest()
    output.mkdir(parents=True, exist_ok=True)
    build_dir = output / 'meson'
    stamp = output / 'environment.sha256'
    # Meson caches machine files. Recreate only its generated directory when
    # compiler, devkit, or flags change; do not reuse a stale cross environment.
    if build_dir.exists() and (not stamp.exists() or stamp.read_text() != identity):
        shutil.rmtree(build_dir)
    cross_file = output / 'optee-ta.txt'
    write_changed(cross_file, cross)
    if not (build_dir / 'build.ninja').exists():
        run('meson', 'setup', build_dir, UPSTREAM, '--cross-file', cross_file,
            '--default-library=static', '--buildtype=minsize', *options)
        write_changed(stamp, identity)
    else:
        configured = json.loads((build_dir / 'meson-info/intro-buildoptions.json').read_text())
        current = {item['name']: str(item['value']).lower() for item in configured}
        if any(current.get(option[2:].split('=', 1)[0]) != option.split('=', 1)[1]
               for option in options):
            run('meson', 'configure', build_dir, *options)
    run('ninja', '-C', build_dir, 'libleancrypto.a')
    adapt_archive(prefix, build_dir / 'libleancrypto.a', output / 'libleancrypto-optee.a')
    write_changed(output / 'lc_optee_features.h',
                  feature_header(build_dir / 'compile_commands.json'))


if __name__ == '__main__':
    try:
        if len(sys.argv) == 3 and sys.argv[1] == '--output-directory':
            print(output_directory(sys.argv[2]))
        elif len(sys.argv) == 1:
            build()
        else:
            raise ValueError('Usage: build.py [--output-directory PATH]')
    except (KeyError, ValueError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(f'error: {error}')
