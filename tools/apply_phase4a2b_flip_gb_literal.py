#!/usr/bin/env python3

"""Run the Phase 4A2B Flip_GB migration with literal regex replacements.

Python's re.sub() interprets backslash escapes in replacement strings.
The original migration contains C string literals such as "...\\n" and
therefore must install replacements through a callable so those backslashes
reach the generated C source unchanged.
"""

from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import re
import sys

MODULE_PATH = Path(__file__).with_name("apply_phase4a2b_flip_gb.py")

spec = spec_from_file_location("phase4a2b_migration", MODULE_PATH)
if spec is None or spec.loader is None:
    raise SystemExit(f"cannot load {MODULE_PATH}")

migration = module_from_spec(spec)
spec.loader.exec_module(migration)


def regex_replace_once_literal(
    text: str,
    pattern: str,
    replacement: str,
    label: str,
) -> str:
    updated, count = re.subn(
        pattern,
        lambda _match: replacement,
        text,
        count=1,
        flags=re.DOTALL,
    )
    if count != 1:
        raise RuntimeError(
            f"{label}: expected exactly one match, found {count}"
        )
    return updated


migration.regex_replace_once = regex_replace_once_literal

try:
    raise SystemExit(migration.main())
except RuntimeError as exc:
    print(f"migration refused: {exc}", file=sys.stderr)
    raise SystemExit(1)
