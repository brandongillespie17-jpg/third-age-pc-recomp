#!/usr/bin/env python3
"""Build from owned ROM/BIOS and a local font into a new private directory.

No old generated sources or caches are used. Never downloads or installs tools,
changes the source checkout, or launches the game. Build output is private.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.dont_write_bytecode=True
from generate_private_assets import generate

ROM_SHA1='0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54'
BIOS_SHA1='300c20df6731a33952ded8c436f7f186d25d3492'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['rom','bios','font','output']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--jobs',type=int,default=4)
    p.add_argument('--cmake-arg',action='append',default=[])
    a=p.parse_args()
    source=Path(__file__).resolve().parents[1];framework=source/'vendor/gbarecomp'
    output=a.output.expanduser().resolve()
    try:
        rom=a.rom.expanduser().resolve(strict=True);bios=a.bios.expanduser().resolve(strict=True);font=a.font.expanduser().resolve(strict=True)
        for input_path,expected in [(rom,ROM_SHA1),(bios,BIOS_SHA1)]:
            if hashlib.sha1(input_path.read_bytes()).hexdigest()!=expected:
                raise ValueError('Input identity mismatch: '+str(input_path))
        if a.jobs<1:raise ValueError('--jobs must be positive')
        # Keep compiled game data and private asset bytes outside all source paths.
        if output==source or source in output.parents or output in source.parents:
            raise ValueError('Output must be separate from the source directory')
        for tool in ['cmake','ninja']:
            if not shutil.which(tool):raise ValueError('Required installed tool missing: '+tool)
        if output.exists():raise ValueError('Output already exists; choose a new directory')
        output.mkdir(parents=True,exist_ok=False)
        marker=output/'BUILD-INCOMPLETE';marker.write_text('Build has not completed. Do not treat this as a release.\n')
        generate(rom,font,output/'private-headers')
        commands=[]
        def run(argv,env=None):
            commands.append(argv);(output/'build-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
            print('+ '+subprocess.list2cmdline(argv),flush=True)
            subprocess.run(argv,check=True,env=env)
        common=['-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DFETCHCONTENT_FULLY_DISCONNECTED=ON',
                '-DGBARECOMP_COMPILER_CACHE=OFF','-DGBARECOMP_TOMLPP_INCLUDE_DIR='+str(framework/'third_party/tomlpp'),*a.cmake_arg]
        run(['cmake','-S',str(framework),'-B',str(output/'tools'),*common])
        run(['cmake','--build',str(output/'tools'),'--target','gba_recompile','--parallel',str(a.jobs)])
        core=output/'tools'/('gba_recompile.exe' if os.name=='nt' else 'gba_recompile')
        if not core.is_file():raise ValueError('Recompiler executable not found: '+str(core))
        env=os.environ.copy();env['GBARECOMP_CORE']=str(core)
        run([sys.executable,str(framework/'tools/cli.py'),'build','--rom',str(rom),'--config',str(source/'third-age.toml'),'--output',str(output/'cartridge'),'--verbose'],env)
        run([str(core),'--bios',str(bios),'--config',str(framework/'bios/gba_bios.toml'),'--out',str(output/'bios')])
        run(['cmake','-S',str(source),'-B',str(output/'runtime'),*common,
             '-DTHIRD_AGE_GENERATED_DIR='+str(output/'cartridge/generated'),
             '-DGBARECOMP_GENERATED_BIOS_DIR='+str(output/'bios'),
             '-DTHIRD_AGE_PRIVATE_INCLUDE_DIR='+str(output/'private-headers'),
             '-DGBARECOMP_SELFHEAL_RECOMPILE_DEFAULT=OFF'])
        run(['cmake','--build',str(output/'runtime'),'--target','third-age-pc','--parallel',str(a.jobs)])
        executable=output/'runtime'/('third-age-pc.exe' if os.name=='nt' else 'third-age-pc')
        if not executable.is_file():raise ValueError('Game executable was not produced')
        (output/'launch.json').write_text(json.dumps({'rom':str(rom),'bios':str(bios)},indent=2)+'\n')
        record={'compiled':True,'game_launched':False,'platform':sys.platform,
                'executable_sha256':hashlib.sha256(executable.read_bytes()).hexdigest(),
                'executable_bytes':executable.stat().st_size,'rom_sha1':ROM_SHA1,'bios_sha1':BIOS_SHA1,
                'font_sha256':hashlib.sha256(font.read_bytes()).hexdigest(),'old_generated_inputs_used':False}
        (output/'build-result.json').write_text(json.dumps(record,indent=2)+'\n')
        marker.unlink()
        print(json.dumps(record,indent=2))
    except (OSError,ValueError,subprocess.CalledProcessError) as e:
        p.exit(1,str(e)+'\n')

if __name__=='__main__':main()
