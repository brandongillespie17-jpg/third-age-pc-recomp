#!/usr/bin/env python3
"""Launch a private trial build; keep saves and diagnostic caches in its folder."""
import argparse
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('trial', type=Path)
args, extra = parser.parse_known_args()
trial = args.trial.expanduser().resolve(strict=True)
config = json.loads((trial / 'launch.json').read_text())
env = os.environ.copy()
env.setdefault('GBARECOMP_COVERAGE_JSON', str(trial / 'coverage.json'))
executable_name = 'third-age-pc.exe' if os.name == 'nt' else 'third-age-pc'
executable = trial / 'runtime' / executable_name
if not executable.is_file():
    executable = trial / 'runtime' / 'Release' / executable_name
if not executable.is_file():
    parser.exit(1, f'Runtime executable not found: {executable_name}\n')
# Preserve the recorded campaign policy; no implicit player compiler requirement.
env['GBARECOMP_FORCE_INTERP'] = '1'
env['GBARECOMP_SELFHEAL_RECOMPILE'] = '0'
env['GBARECOMP_RAM_OVERLAY_HEAL'] = '0'
command = [str(executable), '--rom', config['rom'],
           '--bios', config['bios'], '--save', str(trial / 'third-age.sav')]
# Additional runtime flags replace the default window invocation (e.g. headless tests).
command += extra or ['--window', '--scale', '3']
raise SystemExit(subprocess.call(command, cwd=trial, env=env))
