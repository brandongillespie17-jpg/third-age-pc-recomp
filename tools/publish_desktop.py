#!/usr/bin/env python3
"""Publish only the tested artifacts from this run, in separate platform releases."""
import argparse, hashlib, json, os, subprocess, urllib.request
from pathlib import Path
from prepare_release_assets import extract
from assemble_windows_release import assemble, file_hash, require
from package_linux import package
REPO='brandongillespie17-jpg/third-age-pc-recomp'
OLD_TAG='windows-preview-2026-10-26'
OLD_FILE='third-age-pc-windows-x64-preview-b8b7714f5a38.zip'
def gh(*args):
    return subprocess.check_output(['gh',*args],text=True)
def main(work):
    require(os.environ['GITHUB_REPOSITORY']==REPO,'Unexpected repository')
    commit=os.environ['GITHUB_SHA'];run=os.environ['GITHUB_RUN_ID']
    require(os.environ['GITHUB_REF']=='refs/heads/release/hd-smoothing-20261026','Unexpected publishing branch')
    source=Path(__file__).resolve().parent.parent
    require(not work.exists(),'Fresh publication directory required');work.mkdir(parents=True)
    run_url=f'https://github.com/{REPO}/actions/runs/{run}'
    gh('run','download',run,'--repo',REPO,'--name','third-age-pc-windows-x64-preview-base-'+commit,'--dir',str(work/'windows'))
    gh('run','download',run,'--repo',REPO,'--name','third-age-linux-tested-'+commit,'--dir',str(work/'linux'))
    archive=work/OLD_FILE
    urllib.request.urlretrieve(f'https://github.com/{REPO}/releases/download/{OLD_TAG}/{OLD_FILE}',archive)
    extract(archive,work/'shared',source)
    bases=list((work/'windows').glob('*.zip'));require(len(bases)==1,'Expected exactly one tested base ZIP')
    base=bases[0];digest=file_hash(base)
    require(base.with_suffix('.zip.sha256').read_text(encoding='utf-8-sig').split()[0]==digest,'CI artifact checksum mismatch')
    win=assemble(base,digest,commit,work/'shared/assets/portraits',work/'windows-final',run_url,work/'shared/extras/Ringbearer-original.zip')
    exe=work/'linux/third-age-pc';exe.chmod(0o755)
    package(source,exe,work/'shared',work/'linux-final',commit,run_url,work/'linux')
    linux=list((work/'linux-final').glob('*.tar.gz'));require(len(linux)==1,'Expected one Linux archive')
    releases=[]
    for platform,binary in [('windows',win),('linux',linux[0])]:
        tag=f'{platform}-preview-hd-2026-10-26'
        title=f'Third Age PC — {platform.title()} HD + smoothing preview (2026-10-26)'
        notes=work/(platform+'-notes.md')
        setup=('Extract the entire ZIP to a writable folder and launch `third-age-pc.exe`. Select your own supported BIOS and ROM. SDL2 is included; no compiler or Python is needed.' if platform=='windows' else 'Native Linux x86_64 build for Ubuntu 24.04 or compatible newer distributions (glibc 2.39+). Install SDL2 and a TrueType font (`sudo apt install libsdl2-2.0-0 fonts-dejavu-core`). Extract the tar.gz, then run `./play.sh --bios "/absolute/path/gba_bios.bin" --rom "/absolute/path/game.gba"`. Later launches remember validated paths. No compiler, Python or Wine is needed.')
        notes.write_text(f'''{setup}

- **F10/Escape → Video → HD Preset** toggles the captured HD visuals. **Advanced** adjusts individual panels, portraits, blending, scaling and colors. OFF/ON remembers custom choices. Existing preferences are preserved.
- Resolution, fullscreen, UI size, refresh rate, audio, controls and gameplay preferences remain independent of the HD preset.
- **Display → Camera Smoothing** applies immediately; choose 60/120/144/165 Hz in **Presentation Rate**. Camera interpolation uses the expanded battlefield.
- **Gameplay → Sprite Cache** exposes the separate experimental renderer cache.
- The simulation retains its normal speed. This remains a preview.

Both platforms compiled and passed live SDL menu/preset/persistence tests with dummy drivers in [this CI run]({run_url}). Hardware display smoothness, audio devices and full-campaign gameplay are not covered by those tests; native Windows gameplay acceptance remains pending.

Includes the same 68 approved portraits and unchanged optional font archive as the previous Windows release, with existing notices. No ROM, BIOS or personal saves are included. Extract into a new folder; to resume a campaign, back up and explicitly select your existing save with `--save`.

Source commit: `{commit}`. Each download includes file hashes and build provenance; the adjacent `.sha256` checks the complete archive.
''')
        # A rerun may resume only its own draft; never replace a published release.
        result=subprocess.run(['gh','release','view',tag,'--repo',REPO,'--json','isDraft,targetCommitish'],capture_output=True,text=True)
        if result.returncode==0:
            existing=json.loads(result.stdout)
            require(existing['isDraft'] and existing['targetCommitish']==commit,'Existing release is not this commit draft')
        else:
            gh('release','create',tag,'--repo',REPO,'--target',commit,'--draft','--prerelease','--title',title,'--notes-file',str(notes))
        files=[binary,binary.with_name(binary.name+'.sha256')]
        gh('release','upload',tag,*map(str,files),'--repo',REPO,'--clobber')
        data=json.loads(gh('api',f'repos/{REPO}/releases/tags/{tag}'))
        require(data['draft'] and data['target_commitish']==commit,'Release provenance mismatch')
        assets={a['name']:a for a in data['assets']}
        require(set(assets)=={p.name for p in files},'Unexpected release assets')
        for p in files:
            a=assets[p.name]
            require(a['size']==p.stat().st_size and a.get('digest')=='sha256:'+file_hash(p),'Uploaded asset hash/size mismatch')
        releases.append(tag)
    # Neither release becomes public until both complete packages are verified.
    for tag in releases:
        gh('release','edit',tag,'--repo',REPO,'--draft=false','--prerelease','--latest=false')
        print(f'https://github.com/{REPO}/releases/tag/{tag}')
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--work',required=True,type=Path);main(parser.parse_args().work)
