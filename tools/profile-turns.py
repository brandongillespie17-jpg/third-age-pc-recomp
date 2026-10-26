#!/usr/bin/env python3
"""Profile turn transitions in a new copy of an existing campaign playtest session."""
import argparse
from datetime import datetime,timezone
import hashlib,json,os
from pathlib import Path
import shutil,subprocess,sys,tempfile,zipfile

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-session',type=Path,required=True,help='Existing playtest session containing the campaign save you want to copy.')
    p.add_argument('--no-pc-samples',action='store_true',help='Record state/timing only for lower overhead comparison.')
    a=p.parse_args();source=a.source_session.expanduser().resolve(strict=True)
    meta=json.loads((source/'session.json').read_text())
    if meta.get('format')!='third-age-playtest-v1':raise ValueError('Source is not a campaign playtest session.')
    trial=Path(meta['trial']).resolve(strict=True);binary=trial/'runtime/third-age-pc'
    config=json.loads((trial/'launch.json').read_text())
    rom=Path(config['rom']).expanduser().resolve(strict=True);bios=Path(config['bios']).expanduser().resolve(strict=True)
    if not binary.is_file() or not os.access(binary,os.X_OK):raise ValueError(f'Executable missing: {binary}')
    source_save=source/'third-age.sav';before=digest(source_save)
    if before is None:raise ValueError('Source session has no save; refusing to start a blank campaign.')
    parent=trial.parent/'turn-profiles';parent.mkdir(exist_ok=True)
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    run=Path(tempfile.mkdtemp(prefix=stamp+'-',dir=parent));save=run/'third-age.sav'
    shutil.copy2(source_save,save)
    if digest(save)!=before or digest(source_save)!=before:raise ValueError('Source changed while copying; close other game instances and retry.')
    prefs=source/'third-age-settings.ini'
    if prefs.exists():shutil.copy2(prefs,run/prefs.name)
    env={k:v for k,v in os.environ.items() if not k.startswith('GBARECOMP_')}
    env.update(GBARECOMP_FORCE_INTERP='1',GBARECOMP_SELFHEAL_RECOMPILE='0',GBARECOMP_HEAL_CACHE=str(run/'cache'),GBARECOMP_COVERAGE_JSON=str(run/'coverage.json'),GBARECOMP_THIRD_AGE_PROFILE=str(run/'profile.csv'),GBARECOMP_THIRD_AGE_PC_SAMPLES='0' if a.no_pc_samples else '1')
    print('Using a fresh copy of your campaign save. Your source playtest session is unchanged.',flush=True)
    print('Load the save; capture a player turn start, an enemy turn, and a combat return.',flush=True)
    print('Keep the capture under 8 minutes, avoid fast-forward, then use Overlay > Exit Game.',flush=True)
    print('Any in-game saves in this run belong only to this diagnostic copy.',flush=True)
    print(f'Diagnostics: {run}',flush=True)
    command=[str(binary),'--rom',str(rom),'--bios',str(bios),'--save',str(save),'--window','--scale','3']
    with (run/'runtime.log').open('w') as log:r=subprocess.run(command,cwd=run,env=env,stdout=log,stderr=subprocess.STDOUT)
    result={'format':'third-age-profile-run-v1','exit_code':r.returncode,'binary_sha256':digest(binary),'source_session':str(source),'source_save_sha256_before':before,'source_save_sha256_after':digest(source_save),'source_save_unchanged':digest(source_save)==before,'diagnostic_save_sha256':digest(save),'pc_sampling':not a.no_pc_samples}
    (run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    if not (run/'profile.csv').is_file():raise ValueError(f'No profile was written. Check that the diagnostic patch built successfully and close normally. Log: {run / "runtime.log"}')
    subprocess.run([sys.executable,str(Path(__file__).with_name('analyze-turn-profile.py')),str(run/'profile.csv')],check=True)
    archive=run/'turn-profile-report.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for name in ['profile.csv','profile.summary.json','profile.summary.md','runtime.log','result.json']:
            z.write(run/name,name)
    print(f'Upload this report: {archive}')
    print('The report excludes your ROM, BIOS, and save files.')
    if not result['source_save_unchanged']:print('Source save changed during this run; another game instance may have written to it.')
    return 0 if r.returncode==0 and result['source_save_unchanged'] else 1
if __name__=='__main__':
    try:raise SystemExit(main())
    except (OSError,ValueError,KeyError,subprocess.CalledProcessError) as e:raise SystemExit(f'Profiling stopped: {e}')
