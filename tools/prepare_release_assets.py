#!/usr/bin/env python3
"""Recover only hash-pinned shared artwork from the previous public release."""
import argparse, hashlib, json, shutil, zipfile
from pathlib import Path
from assemble_windows_release import MANIFEST_SHA256, FONT_ARCHIVE_SHA256, file_hash, require
PREVIOUS_ZIP_SHA256='6a8086fae52a2246c336826b2171bec11ef800b5e9a9dfe076902625c45d3356'
PREVIOUS_ROOT='third-age-pc-windows-x64-preview-b8b7714f5a38/'
def extract(archive, output, source):
    require(file_hash(archive)==PREVIOUS_ZIP_SHA256,'Previous release ZIP checksum mismatch')
    manifest=source/'assets/PORTRAIT-MANIFEST.json'
    require(file_hash(manifest)==MANIFEST_SHA256,'Portrait manifest changed')
    rows=json.loads(manifest.read_text())['files']
    require(len(rows)==68,'Expected 68 existing approved portraits')
    require(not output.exists(),'Shared asset output must be fresh')
    selected={r['file']:(r['sha256'],r['bytes']) for r in rows}
    selected['extras/Ringbearer-original.zip']=(FONT_ARCHIVE_SHA256,132332)
    with zipfile.ZipFile(archive) as z:
        names=z.namelist();require(len(names)==len(set(names)),'Duplicate ZIP members')
        for name,(digest,size) in selected.items():
            member=z.getinfo(PREVIOUS_ROOT+name)
            require(member.file_size==size,'Unexpected asset length: '+name)
            with z.open(member) as stream:
                require(hashlib.file_digest(stream,'sha256').hexdigest()==digest,'Asset checksum mismatch: '+name)
        output.mkdir(parents=True)
        for name in selected:
            dest=output/name;dest.parent.mkdir(parents=True,exist_ok=True)
            with z.open(PREVIOUS_ROOT+name) as src,dest.open('xb') as dst:shutil.copyfileobj(src,dst)
    print('Recovered exactly 68 approved portraits and the optional unchanged font archive')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--archive',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,required=True);a=p.parse_args();extract(a.archive,a.output,a.source)
