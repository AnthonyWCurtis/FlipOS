#!/usr/bin/env python3
"""Offline integrity and ELF .modinfo preflight for legacy release inputs.

No target code runs and no module loads. A matching vermagic is *not* proof of
kernel ABI compatibility: device-side uname/config/symbol verification remains.
"""

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path


class AuditError(ValueError):
    pass


def digest(path):
    hasher = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            hasher.update(chunk)
    return hasher.hexdigest()


def checked_digest(path, expected):
    if not isinstance(expected, str) or not re.fullmatch(r'[0-9a-f]{64}', expected):
        raise AuditError('invalid pinned SHA-256')
    if not path.is_file() or path.is_symlink():
        raise AuditError(f'missing/unsafe regular file: {path}')
    actual = digest(path)
    if actual != expected:
        raise AuditError(f'SHA-256 mismatch: {path.name}: {actual} != {expected}')
    return actual


def section_bytes(data, offset, size, label):
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise AuditError(f'out-of-range ELF {label}')
    return data[offset:offset + size]


def module_info(path):
    data = path.read_bytes()
    if len(data) < 64 or data[:4] != b'\x7fELF':
        raise AuditError('not an ELF object')
    if data[4] != 2 or data[5] != 1 or data[6] != 1:
        raise AuditError('module must be ELF64 little-endian')
    (etype, machine, eversion, _entry, _phoff, shoff, _flags,
     ehsize, _phentsize, _phnum, shentsize, shnum, shstrndx) = struct.unpack_from('<HHIQQQIHHHHHH', data, 16)
    if (etype, machine, eversion) != (1, 183, 1):
        raise AuditError('module must be AArch64 relocatable ELF')
    if ehsize != 64 or shentsize != 64 or shnum < 3 or shstrndx >= shnum:
        raise AuditError('invalid ELF section table layout')
    table = section_bytes(data, shoff, shentsize * shnum, 'section table')
    sections = [struct.unpack_from('<IIQQQQIIQQ', table, i * shentsize)
                for i in range(shnum)]
    shstr = sections[shstrndx]
    names = section_bytes(data, shstr[4], shstr[5], 'section names')

    def get_name(offset):
        if offset >= len(names):
            raise AuditError('invalid section name offset')
        stop = names.find(b'\0', offset)
        if stop < 0:
            raise AuditError('unterminated section name')
        return names[offset:stop]

    metadata = [sec for sec in sections if get_name(sec[0]) == b'.modinfo']
    if len(metadata) != 1 or metadata[0][1] != 1:
        raise AuditError('missing/invalid .modinfo section')
    section = metadata[0]
    raw = section_bytes(data, section[4], section[5], '.modinfo')
    if not raw or raw[-1] != 0:
        raise AuditError('invalid .modinfo termination')
    entries = {}
    for record in raw.split(b'\0'):
        if not record:
            continue
        key, separator, value = record.partition(b'=')
        if not separator:
            raise AuditError('invalid .modinfo record')
        try:
            name = key.decode('ascii')
            value_text = value.decode('ascii')
        except UnicodeError as exc:
            raise AuditError('non-ASCII .modinfo field') from exc
        if name in {'name', 'vermagic', 'depends', 'license'} and name in entries:
            raise AuditError(f'duplicate .modinfo {name}')
        entries[name] = value_text
    for required in ('name', 'vermagic', 'license'):
        if not entries.get(required):
            raise AuditError(f'missing .modinfo {required}')
    return entries


def read_lock(path):
    config = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(config, dict) or config.get('format') != 1:
        raise AuditError('unsupported release module lock')
    if not isinstance(config.get('modules'), dict) or len(config['modules']) != 4:
        raise AuditError('release module lock must contain four modules')
    return config


def verify(locked, archive, directory):
    if archive is not None:
        actual = checked_digest(archive, locked['archive_sha256'])
        print(f'ARCHIVE_SHA256_OK {actual} {archive.name}')
    if directory is None:
        return
    expected_kernel = locked['expected_kernel_base']
    if not isinstance(expected_kernel, str) or not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', expected_kernel):
        raise AuditError('invalid expected kernel base')
    expected_vermagic = locked['expected_vermagic']
    if not isinstance(expected_vermagic, str) or not expected_vermagic.startswith(expected_kernel + ' '):
        raise AuditError('invalid pinned module vermagic')
    vermagics = set()
    for filename, expected_sha in sorted(locked['modules'].items()):
        if not re.fullmatch(r'flip_[a-z0-9_]+\.ko', filename):
            raise AuditError(f'invalid locked module filename: {filename}')
        path = directory / filename
        sha = checked_digest(path, expected_sha)
        modinfo = module_info(path)
        if modinfo['name'] != filename[:-3]:
            raise AuditError(f'module name mismatch for {filename}: {modinfo["name"]}')
        vermagic = modinfo['vermagic']
        release = vermagic.split(' ', 1)[0]
        if not re.fullmatch(re.escape(expected_kernel) + r'(?:[+._-][A-Za-z0-9.+_-]+)?', release):
            raise AuditError(f'unexpected kernel version in {filename}: {vermagic}')
        if not re.search(r'\baarch64\b', vermagic):
            raise AuditError(f'not AArch64 vermagic: {filename}')
        if modinfo['license'] not in {'GPL', 'GPL v2', 'Dual BSD/GPL', 'Dual MIT/GPL'}:
            raise AuditError(f'unexpected module license: {filename}')
        if vermagic != expected_vermagic:
            raise AuditError(f'unexpected module vermagic: {filename}: {vermagic}')
        vermagics.add(vermagic)
        print(f'MODULE_ABI_PRECHECK_OK {filename} sha256={sha} '
              f'vermagic={vermagic!r} depends={modinfo.get("depends", "")!r} '
              f'license={modinfo["license"]!r}')
    if len(vermagics) != 1:
        raise AuditError('legacy modules disagree on their kernel vermagic')
    print('MODULE_VERMAGIC_CONSISTENT (target device uname, config and symbol CRCs NOT verified)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lock', required=True, type=Path)
    parser.add_argument('--archive', type=Path, help='check pinned archive BEFORE tar reads it')
    parser.add_argument('--modules', type=Path, help='verify four previously extracted modules')
    args = parser.parse_args()
    if args.archive is None and args.modules is None:
        parser.error('at least --archive or --modules is required')
    try:
        verify(read_lock(args.lock), args.archive, args.modules)
    except (AuditError, OSError, ValueError, KeyError) as exc:
        print(f'MODULE_RELEASE_INPUTS_REJECTED: {exc}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
