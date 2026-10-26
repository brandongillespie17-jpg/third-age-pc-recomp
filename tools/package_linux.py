#!/usr/bin/env python3
"""Package the tested Linux portable runtime; no ROM, BIOS or personal state."""
import argparse, hashlib, io, json, os, re, shutil, stat, subprocess, tarfile
from pathlib import Path
from assemble_windows_release import MANIFEST_SHA256, FONT_MANIFEST_SHA256, portrait_inputs, font_input, file_hash, require
NOTICES={
 'DISTRIBUTION-NOTICE.md':'notices/DISTRIBUTION-NOTICE.md',
 'LICENSING-INVENTORY.md':'notices/LICENSING-INVENTORY.md',
 'vendor/gbarecomp/LICENSE':'notices/gbarecomp-LICENSE.txt',
 'vendor/gbarecomp/THIRD_PARTY_ATTRIBUTION.md':'notices/THIRD_PARTY_ATTRIBUTION.md',
 'vendor/gbarecomp/third_party/MPL-2.0.txt':'notices/MPL-2.0.txt',
 'vendor/gbarecomp/src/runtime/bios_hle.cpp':'source/mpl/bios_hle.cpp',
 'vendor/gbarecomp/src/runtime/bios_hle.h':'source/mpl/bios_hle.h',
 'assets/PORTRAIT-MANIFEST.json':'assets/PORTRAIT-MANIFEST.json',
 'assets/PORTRAIT-NOTICE.md':'assets/PORTRAIT-NOTICE.md',
 'assets/FONT-MANIFEST.json':'assets/FONT-MANIFEST.json',
 'assets/FONT-NOTICE.md':'extras/Ringbearer-NOTICE.txt',
 'vendor/gbarecomp/external/arm-recomp-core/LICENSE':'notices/arm-recomp-core-LICENSE.txt',
 'vendor/gbarecomp/external/rbengine/LICENSE':'notices/rbengine-LICENSE.txt',
 'vendor/gbarecomp/external/recomp-net/LICENSE':'notices/recomp-net-LICENSE.txt',
 'vendor/gbarecomp/third_party/tomlpp/LICENSE':'notices/tomlpp-LICENSE.txt'}
README='''Third Age PC - Linux x86_64 preview (camera smoothing controls)

Extract the entire tar.gz to a writable folder. Keep assets beside the executable.
Built on Ubuntu 24.04; use Ubuntu 24.04 or a compatible newer x86_64 distribution.
System runtime requirements: glibc 2.39+, SDL2 and a TrueType font. The C++ runtime
is statically linked. On Ubuntu: sudo apt install libsdl2-2.0-0 fonts-dejavu-core
No compiler, Python or Wine is needed to play this native Linux executable.

First launch from a terminal, supplying absolute paths to your own files:
  ./play.sh --bios "/absolute/path/gba_bios.bin" --rom "/absolute/path/game.gba"
Later launches remember validated paths in bios.cfg / rom.cfg: ./play.sh
Linux does not currently have the Windows file-selection dialogs.
Supported BIOS SHA1: 300c20df6731a33952ded8c436f7f186d25d3492 (16384 bytes)
Supported ROM SHA1:  0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54 (16777216 bytes)
New saves: saves/third-age.sav. Settings live beside the selected save.
To resume an existing campaign, back it up and pass --save with its absolute path.
Do not overwrite an original installation when extracting this preview.

F10/Escape > Display > Camera Smoothing: ON/OFF, applies immediately and persists.
Presentation Rate: 60 / 120 / 144 / 165 Hz. Match the display's refresh rate.
Video > HD Preset toggles the captured HD visuals without changing display settings.
Video > Advanced adjusts individual visuals and colors; OFF/ON remembers your tweaks.
Camera interpolation uses Video > Advanced > Expanded Battlefield.
Gameplay > Sprite Cache: separate experimental rendering cache, saved automatically.
Video > Smooth Scaling controls pixel filtering, separately from camera motion.
No restart or environment variables are needed. The simulation keeps its normal rate.

The 68 existing approved portrait BMPs are included unchanged. The original
Ringbearer archive in extras is optional and is not installed/loaded automatically.
The default interface uses a system font. All existing notices remain included.
No ROM, BIOS, personal save, game-derived translation or film audio/video is supplied.

CI compile and dummy-SDL menu/persistence tests passed. Dummy-SDL tests do not
validate physical display smoothness, audio hardware or full campaign gameplay.
See BUILD-INFO.json for the exact source commit and dependency/build information.
'''
LAUNCHER='''#!/bin/sh
set -eu
release_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$release_dir"
exec "$release_dir/third-age-pc" "$@"
'''
def package(source, executable, assets, output, commit, run_url, build):
    require(re.fullmatch('[0-9a-f]{40}',commit) is not None,'Expected full commit')
    require(not output.exists(),'Output directory must be fresh')
    data=executable.read_bytes()
    require(data[:6]==b'\x7fELF\x02\x01' and int.from_bytes(data[18:20],'little')==62,'Expected x86_64 ELF')
    require(subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()==commit,'Wrong checkout commit')
    require(not subprocess.check_output(['git','-C',str(source),'status','--porcelain'],text=True).strip(),'Source checkout must be clean')
    cache=(build/'CMakeCache.txt').read_text()
    for flag in ['THIRD_AGE_PORTABLE_RELEASE:BOOL=ON','THIRD_AGE_BUILD_PRESENTATION_TESTS:BOOL=ON','GBARECOMP_SELFHEAL_RECOMPILE_DEFAULT:BOOL=OFF']:
        require(flag in cache,'Missing build option: '+flag)
    require(re.search(r'^THIRD_AGE_PRIVATE_INCLUDE_DIR:[^=]+=\s*$',cache,re.M) is not None,'Unexpected private inputs')
    deps=subprocess.check_output(['ldd',str(executable)],text=True)
    require('not found' not in deps and 'libSDL2' in deps,'Missing runtime dependencies')
    direct=subprocess.check_output(['readelf','-d',str(executable)],text=True)
    require('libstdc++' not in direct and 'libgcc_s' not in direct,'C++ runtime must be statically linked')
    require(file_hash(source/'assets/PORTRAIT-MANIFEST.json')==MANIFEST_SHA256,'Portrait pin mismatch')
    require(file_hash(source/'assets/FONT-MANIFEST.json')==FONT_MANIFEST_SHA256,'Font pin mismatch')
    contents={dest:(source/src).read_bytes() for src,dest in NOTICES.items()}
    portraits=portrait_inputs(contents,assets/'assets/portraits')
    font=font_input(contents,assets/'extras/Ringbearer-original.zip')
    output.mkdir(parents=True);root='third-age-pc-linux-x86_64-preview-'+commit[:12]
    contents.update({'third-age-pc':data,'play.sh':LAUNCHER.encode(),'README.txt':README.encode(),'extras/Ringbearer-original.zip':font})
    versions=subprocess.check_output(['readelf','--version-info',str(executable)],text=True)
    info={'commit':commit,'target':'third-age-pc','platform':'Linux x86_64','runner':'ubuntu-24.04','ciRunUrl':run_url,
          'configuration':'Release; portable runtime; static libstdc++/libgcc; system SDL2',
          'elfDependencies':deps,'glibcVersions':sorted(set(re.findall(r'GLIBC_[0-9.]+',versions))),
          'tests':['clean CI build without private game inputs','real SDL menu/persistence regression with dummy drivers','ELF and dependency audit','archive path/mode/content hash round-trip'],
          'portraitCount':68,'portraitManifestSha256':MANIFEST_SHA256,'fontArchiveIncluded':True,'defaultFont':'system font',
          'limitations':['physical display/audio and full-campaign acceptance not performed by CI']}
    contents['BUILD-INFO.json']=(json.dumps(info,indent=2)+'\n').encode()
    hashes={name:hashlib.sha256(data).hexdigest() for name,data in contents.items()}
    hashes.update({n:file_hash(p) for n,p in portraits.items()})
    contents['SHA256SUMS.txt']=''.join(f'{hashes[n]}  {n}\n' for n in sorted(hashes)).encode()
    hashes['SHA256SUMS.txt']=hashlib.sha256(contents['SHA256SUMS.txt']).hexdigest()
    archive=output/(root+'.tar.gz');names=set(contents)|set(portraits)
    with tarfile.open(archive,'w:gz',compresslevel=6) as tar:
        for name in sorted(names):
            mode=0o755 if name in {'third-age-pc','play.sh'} else 0o644
            entry=tarfile.TarInfo(root+'/'+name);entry.mode=mode;entry.mtime=0
            if name in contents:
                entry.size=len(contents[name]);tar.addfile(entry,io.BytesIO(contents[name]))
            else:
                entry.size=portraits[name].stat().st_size
                with portraits[name].open('rb') as f:tar.addfile(entry,f)
    with tarfile.open(archive) as tar:
        require(set(tar.getnames())=={root+'/'+n for n in names},'Archive allowlist mismatch')
        for entry in tar:
            name=entry.name[len(root)+1:];require(entry.isfile(),'Non-file member')
            require(entry.mode==(0o755 if name in {'third-age-pc','play.sh'} else 0o644),'Wrong file mode')
            with tar.extractfile(entry) as f:require(hashlib.file_digest(f,'sha256').hexdigest()==hashes[name],'Archive hash mismatch: '+name)
    (output/(archive.name+'.sha256')).write_text(f'{file_hash(archive)}  {archive.name}\n')
    print(archive)
if __name__=='__main__':
    p=argparse.ArgumentParser()
    for arg in ['source','executable','assets','output','build']:p.add_argument('--'+arg,type=Path,required=True)
    p.add_argument('--commit',required=True);p.add_argument('--run-url',required=True)
    a=p.parse_args();package(a.source,a.executable,a.assets,a.output,a.commit,a.run_url,a.build)
