#!/usr/bin/env python3
"""Activate the chosen font in the existing Windows preview, preserving EXE/DLL bytes."""
import argparse, json, os, subprocess, urllib.request, zipfile
from pathlib import Path
from assemble_windows_release import assemble, file_hash, require
from prepare_release_assets import extract
REPO='brandongillespie17-jpg/third-age-pc-recomp'
COMMIT='4beaa8d235b3e40549c675fceb1d0218431459e9'
BUILD_RUN='40405711931'
TAG='windows-preview-hd-2026-10-26'
OLD_NAME='third-age-pc-windows-x64-preview-4beaa8d235b3.zip'
OLD_HASH='a389a3427a97af3704f479012a7d1078334772937844d3673ad5fed7ce7bda12'
def gh(*args):return subprocess.check_output(['gh',*args],text=True,encoding='utf-8')
def release():return json.loads(gh('api',f'repos/{REPO}/releases/tags/{TAG}'))
def main(work):
    require(os.name=='nt','Run the font selection check on native Windows')
    require(os.environ['GITHUB_REPOSITORY']==REPO and os.environ['GITHUB_REF']=='refs/heads/release/windows-font-20261026','Unexpected publication context')
    require(not work.exists(),'Fresh directory required');work.mkdir(parents=True)
    source=Path(__file__).resolve().parent.parent
    current=release();require(not current['draft'] and current['target_commitish']==COMMIT,'Release source changed')
    existing={a['name']:a for a in current['assets']}
    font_name=OLD_NAME.replace('.zip','-ringbearer.zip')
    previous=existing.get(OLD_NAME) or existing.get(font_name)
    require(previous and previous['digest'] in {'sha256:'+OLD_HASH,'sha256:87489d656026cb122120995447290d625a0eecd2902a2adfcd5589988b3cda26'},'Previous Windows ZIP changed')
    run=json.loads(gh('api',f'repos/{REPO}/actions/runs/{BUILD_RUN}'));require(run['head_sha']==COMMIT,'Build commit mismatch')
    jobs=json.loads(gh('api',f'repos/{REPO}/actions/runs/{BUILD_RUN}/jobs'))['jobs']
    require(any(j['name']=='windows / windows-preview' and j['conclusion']=='success' for j in jobs),'Windows build/tests did not pass')
    gh('run','download',BUILD_RUN,'--repo',REPO,'--name','third-age-pc-windows-x64-preview-base-'+COMMIT,'--dir',str(work/'base'))
    original=work/'previous.zip'
    urllib.request.urlretrieve(f'https://github.com/{REPO}/releases/download/windows-preview-2026-10-26/third-age-pc-windows-x64-preview-b8b7714f5a38.zip',original)
    extract(original,work/'shared',source)
    bases=list((work/'base').glob('*.zip'));require(len(bases)==1,'Expected one tested base')
    base=bases[0];digest=file_hash(base)
    require(base.with_suffix('.zip.sha256').read_text(encoding='utf-8-sig').split()[0]==digest,'Base checksum mismatch')
    package=assemble(base,digest,COMMIT,work/'shared/assets/portraits',work/'final',f'https://github.com/{REPO}/actions/runs/{BUILD_RUN}',work/'shared/extras/Ringbearer-original.zip')
    # Exercise the actual unchanged runtime loader against the final ZIP contents.
    probe_root=work/'probe';probe_root.mkdir()
    with zipfile.ZipFile(package) as archive:
        for name in archive.namelist():
            relative='/'.join(name.split('/')[1:])
            if relative.startswith('assets/fonts/'):
                out=probe_root/relative;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(archive.read(name))
    probe=work/'font-selection.exe'
    subprocess.run(['cl.exe','/nologo','/std:c++20','/EHsc','/MT','/UNDEBUG','/I'+str(source/'vendor/gbarecomp/src/runtime'),str(source/'tests/font-selection.cpp'),'/Fe:'+str(probe)],cwd=work,check=True)
    env=dict(os.environ);env.pop('THIRD_AGE_FONT',None)
    subprocess.run([str(probe),str(probe_root)],env=env,check=True)
    # Upload new, distinctly named assets before replacing the old download links.
    files=[package,package.with_name(package.name+'.sha256')]
    gh('release','upload',TAG,*map(str,files),'--repo',REPO,'--clobber')
    uploaded=release();assets={a['name']:a for a in uploaded['assets']}
    for p in files:
        a=assets[p.name];require(a['size']==p.stat().st_size and a['digest']=='sha256:'+file_hash(p),'Uploaded checksum mismatch')
    notes=work/'notes.md'
    prior_body=current['body']
    if prior_body.startswith('**Font correction:**'):prior_body=prior_body.split('\n\n',1)[1]
    notes.write_text('**Font correction:** Ringbearer now loads automatically from the included `assets/fonts/ringbearer.ttf`. No manual extraction, font installation or environment variable is needed. All original font files, READMEs and the original archive are preserved unchanged. EXE and SDL2 DLL bytes are unchanged from the tested HD build.\n\n'+prior_body.replace('unchanged optional font archive','complete unchanged font distribution'))
    gh('release','edit',TAG,'--repo',REPO,'--notes-file',str(notes))
    # Only the two superseded Windows assets are removed after verification.
    for name in [OLD_NAME,OLD_NAME+'.sha256']:
        if name in assets:gh('release','delete-asset',TAG,name,'--repo',REPO,'--yes')
    final=release();require({a['name'] for a in final['assets']}=={p.name for p in files},'Unexpected final release assets')
    print(json.dumps({'release':final['html_url'],'font':'Ringbearer Medium v1.00','nativeWindowsLoaderTest':'PASS','assets':[{k:a[k] for k in ['name','size','digest','browser_download_url']} for a in final['assets']]},indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--work',required=True,type=Path);main(p.parse_args().work)
