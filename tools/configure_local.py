#!/usr/bin/env python3
"""Configure preserved modified sources using explicit local private inputs.

Never clones, resets, installs or updates dependencies. Does not generate ROM or
BIOS code. Requires private generated inputs described in docs/BUILD.md.
"""
import argparse
from pathlib import Path
import subprocess

def cmake_command(source, build, cartridge, bios, private_headers, tomlpp, extra):
    required = [bios/'bios_recompiled.cpp', tomlpp/'toml.hpp',
                private_headers/'third_age_portrait_patterns.private.inc',
                private_headers/'third_age_ringbearer_data.h',
                private_headers/'third_age_native_decode.private.inc']
    for p in required:
        if not p.is_file():
            raise ValueError('Missing required local input: '+str(p))
    if len(list(cartridge.glob('recompiled_[0-9][0-9][0-9].cpp'))) < 2:
        raise ValueError('At least two generated cartridge shards are required.')
    if (cartridge/'recompiled.cpp').exists():
        raise ValueError('Stale monolithic cartridge source is unsupported.')
    if build.exists():
        raise ValueError('Use a new build directory to avoid stale source/ABI caches.')
    return ['cmake','-S',str(source),'-B',str(build),'-G','Ninja',
            '-DCMAKE_BUILD_TYPE=Release',
            '-DGBARECOMP_ROOT='+str(source/'vendor/gbarecomp'),
            '-DTHIRD_AGE_GENERATED_DIR='+str(cartridge),
            '-DGBARECOMP_GENERATED_BIOS_DIR='+str(bios),
            '-DTHIRD_AGE_PRIVATE_INCLUDE_DIR='+str(private_headers),
            '-DGBARECOMP_TOMLPP_INCLUDE_DIR='+str(tomlpp),
            '-DGBARECOMP_SELFHEAL_RECOMPILE_DEFAULT=OFF',
            '-DFETCHCONTENT_FULLY_DISCONNECTED=ON',*extra]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['build-dir','cartridge','bios','private-headers','tomlpp']:
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--compile',action='store_true')
    p.add_argument('--cmake-arg',action='append',default=[])
    a=p.parse_args()
    source=Path(__file__).resolve().parents[1]
    try:
        command=cmake_command(source,a.build_dir.resolve(),a.cartridge.resolve(),a.bios.resolve(),a.private_headers.resolve(),a.tomlpp.resolve(),a.cmake_arg)
        subprocess.run(command,check=True)
        if a.compile:
            subprocess.run(['cmake','--build',str(a.build_dir.resolve()),'--target','third-age-pc','--parallel','4'],check=True)
    except (ValueError,OSError,subprocess.CalledProcessError) as e:
        p.exit(1,str(e)+'\n')

if __name__=='__main__':
    main()
