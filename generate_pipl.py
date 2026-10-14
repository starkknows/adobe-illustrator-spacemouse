# SPDX-License-Identifier: MIT
# Copyright (c) 2026 SpaceNavigator for Illustrator contributors
"""Run Adobe's PiPL generator without shell-dependent JSON quoting."""
import json
from pathlib import Path
import runpy
import sys


def main():
    root = Path(__file__).resolve().parent
    generator = root / 'sdk' / 'Adobe Illustrator 2026 SDK' / 'tools' / 'pipl' / 'create_pipl.py'
    if not generator.is_file():
        raise SystemExit('Adobe PiPL generator not found. Check the SDK layout in README.md.')
    sys.path.insert(0, str(generator.parent))
    sys.argv = [str(generator), '-input', json.dumps([{'name': 'IllustratorNavigator'}])]
    runpy.run_path(str(generator), run_name='__main__')


if __name__ == '__main__':
    main()
