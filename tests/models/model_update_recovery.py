"""Disposable model of a proposed FlipOS interrupted-update state machine.

THIS IS NOT PRODUCTION FIRMWARE. It uses a synthetic filesystem and the
host's atomic rename semantics; it is NOT evidence of SD/FAT crash durability.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
from typing import Mapping


class RecoveryError(RuntimeError):
    """An update must stop and leave the original recovery evidence intact."""


def digest(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def allowed_relpath(name: str) -> bool:
    path = PurePosixPath(name)
    if (not name or name.startswith('/') or '\\' in name or
            '..' in name.split('/') or '//' in name or path.as_posix() != name):
        return False
    return name == 'VERSION' or name == 'updater-playtest' or (
        len(path.parts) == 2 and path.parts[0] == 'bin' and path.parts[1] != '.'
    ) or (
        len(path.parts) == 2 and path.parts[0] == 'lib' and name.endswith('.ko')
    )


class UpdateRecoveryModel:
    """A deliberately simple safety oracle, not a drop-in updater implementation."""

    def __init__(self, root: Path):
        self.root = root
        self.active = root / 'active'
        self.tx = root / 'transaction'
        self.marker = self.tx / 'journal.json'
        self.active.mkdir(parents=True, exist_ok=True)

    def _read_journal(self) -> dict:
        try:
            raw = json.loads(self.marker.read_text(encoding='utf-8'))
        except (OSError, ValueError) as exc:
            raise RecoveryError('invalid/missing transaction marker') from exc
        if not isinstance(raw, dict) or raw.get('format') != 1 or raw.get('status') not in ('pending', 'committed'):
            raise RecoveryError('invalid transaction format or status')
        files = raw.get('files')
        if not isinstance(files, list) or not files:
            raise RecoveryError('invalid transaction file set')
        seen = set()
        for entry in files:
            if not isinstance(entry, dict) or not allowed_relpath(entry.get('name', '')):
                raise RecoveryError('unsafe file entry')
            name = entry['name']
            if name in seen:
                raise RecoveryError('duplicate entry')
            seen.add(name)
            for field in ('old', 'new'):
                checksum = entry.get(field)
                if checksum is not None and (not isinstance(checksum, str) or len(checksum) != 64 or any(c not in '0123456789abcdef' for c in checksum)):
                    raise RecoveryError('invalid digest')
            if entry.get('new') is None:
                raise RecoveryError('missing required new digest')
        return raw

    def seed(self, name: str, data: bytes) -> None:
        if not allowed_relpath(name):
            raise ValueError('unsafe test name')
        dest = self.active / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)

    def prepare(self, changes: Mapping[str, bytes]) -> None:
        if self.tx.exists():
            raise RecoveryError('previous transaction not reconciled')
        if not changes:
            raise ValueError('empty update')
        entries = []
        self.tx.mkdir(parents=True)
        for name, data in changes.items():
            if not allowed_relpath(name):
                raise RecoveryError('unsafe update name')
            current = self.active / name
            old = current.read_bytes() if current.exists() else None
            if old is not None:
                path = self.tx / 'backup' / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(old)
            incoming = self.tx / 'incoming' / name
            incoming.parent.mkdir(parents=True, exist_ok=True)
            incoming.write_bytes(data)
            entries.append({'name': name, 'old': digest(old) if old is not None else None, 'new': digest(data)})
        record = {'format': 1, 'status': 'pending', 'files': entries}
        temp = self.tx / 'journal.json.tmp'
        temp.write_text(json.dumps(record, sort_keys=True), encoding='utf-8')
        os.replace(temp, self.marker)  # Model only; no FAT-directory sync guarantee.

    def apply_one(self, name: str) -> None:
        record = self._read_journal()
        if record['status'] != 'pending':
            raise RecoveryError('not pending')
        entry = next((x for x in record['files'] if x['name'] == name), None)
        if entry is None:
            raise RecoveryError('file not journalled')
        staged = self.tx / 'incoming' / name
        if not staged.is_file() or digest(staged.read_bytes()) != entry['new']:
            raise RecoveryError('invalid staged bytes')
        dest = self.active / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        part = dest.with_name(dest.name + '.new')
        shutil.copyfile(staged, part)
        os.replace(part, dest)

    def commit(self) -> None:
        record = self._read_journal()
        if record['status'] != 'pending':
            raise RecoveryError('not pending')
        for entry in record['files']:
            current = self.active / entry['name']
            if not current.is_file() or digest(current.read_bytes()) != entry['new']:
                raise RecoveryError('new generation not fully installed')
        record['status'] = 'committed'
        temp = self.tx / 'journal.json.tmp'
        temp.write_text(json.dumps(record, sort_keys=True), encoding='utf-8')
        os.replace(temp, self.marker)

    def boot_reconcile(self, *, writable: bool = True) -> str:
        if not self.tx.exists():
            return 'clean'
        if not self.marker.exists():
            # Before first mutation, staging may be incomplete. A production
            # boot checker needs a separate trusted boot path to prove this.
            raise RecoveryError('orphaned transaction; refuse to guess')
        record = self._read_journal()
        if record['status'] == 'committed':
            for entry in record['files']:
                dest = self.active / entry['name']
                if not dest.is_file() or digest(dest.read_bytes()) != entry['new']:
                    raise RecoveryError('committed generation invalid')
            return 'use-new'
        if not writable:
            raise RecoveryError('read-only medium; cannot safely repair')
        # Verify every backup *before* touching a single active file.
        for entry in record['files']:
            backup = self.tx / 'backup' / entry['name']
            if entry['old'] is None:
                if backup.exists():
                    raise RecoveryError('unexpected backup for previously absent file')
            elif not backup.is_file() or digest(backup.read_bytes()) != entry['old']:
                raise RecoveryError('backup unavailable or corrupt')
        # Keep backups intact throughout restoration; reboot can retry.
        for entry in record['files']:
            dest = self.active / entry['name']
            if entry['old'] is None:
                dest.unlink(missing_ok=True)
            else:
                dest.parent.mkdir(parents=True, exist_ok=True)
                partial = dest.with_name(dest.name + '.restore-tmp')
                shutil.copyfile(self.tx / 'backup' / entry['name'], partial)
                os.replace(partial, dest)
        for entry in record['files']:
            dest = self.active / entry['name']
            if entry['old'] is None:
                if dest.exists():
                    raise RecoveryError('removal was not completed')
            elif not dest.is_file() or digest(dest.read_bytes()) != entry['old']:
                raise RecoveryError('restoration failed validation')
        # Models success; production needs write barriers and crash-safe cleanup.
        shutil.rmtree(self.tx)
        return 'rolled-back'
