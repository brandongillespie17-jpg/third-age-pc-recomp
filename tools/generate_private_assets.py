#!/usr/bin/env python3
"""Generate private portrait signatures from an owned ROM and embed a local TTF.

No game pixels or font bytes are included in this script. Output is private and
must never be committed or distributed. Decoder follows the project's recovered
original-portrait exporter: tiled 4bpp resources with raw or BIOS RLE tile data.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.dont_write_bytecode=True
from generate_decode import generate_decode

ROM_SHA1='0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54'
FONT_SHA256='22220f17fdcec646bc8d0597421f4a589022f93753b48ce1bff6fbb23ceb477b'

def portrait_pixels(rom, kind, unit_type, size):
    def read(address, count):
        offset=address-0x08000000
        if offset<0 or offset+count>len(rom):
            raise ValueError('Portrait resource outside ROM')
        return int.from_bytes(rom[offset:offset+count],'little')
    table=read(0x08a0c188+4*(kind-5),4)
    src=read(table+0xe8*unit_type+(0xe4 if size==40 else 0xe0),4)
    flags,extra=read(src,1),read(src+1,1)
    tiles=size//8
    if flags not in (0x1a,0x3a) or extra&1 or read(src+34,2)!=tiles or read(src+36,2)!=tiles:
        raise ValueError('Unsupported portrait resource layout')
    # Tile bytes start on a 4-byte boundary; the stored-size halfword precedes it.
    stored_at=((src+38+tiles*tiles*2+2+3)//4)*4-2
    stored=read(stored_at,2);at=stored_at+2;art=bytearray()
    if flags==0x1a:
        if not 0<stored<=tiles*tiles*32 or stored%32:
            raise ValueError('Invalid raw tile count')
        art.extend(read(at+i,1) for i in range(stored))
    else:
        end=at+stored;header=read(at,4);at+=4;length=header>>8
        if header&255!=0x30 or not 0<length<=tiles*tiles*32 or length%32 or stored<4:
            raise ValueError('Invalid RLE header')
        while len(art)<length:
            if at>=end:raise ValueError('Truncated RLE control')
            control=read(at,1);at+=1
            count=(control&127)+(3 if control&128 else 1)
            if len(art)+count>length:raise ValueError('RLE output overrun')
            if control&128:
                if at>=end:raise ValueError('Truncated RLE repeat')
                art.extend([read(at,1)]*count);at+=1
            else:
                if at+count>end:raise ValueError('Truncated RLE literal')
                art.extend(read(at+i,1) for i in range(count));at+=count
    pixels=[]
    for y in range(size):
        for x in range(size):
            entry=read(src+38+2*((y//8)*tiles+x//8),2);tile=entry&1023
            if tile>=len(art)//32:raise ValueError('Tile index outside resource')
            px=7-x%8 if entry&1024 else x%8;py=7-y%8 if entry&2048 else y%8
            index=(art[tile*32+py*4+px//2]>>(4*(px%2)))&15
            color=read(src+2+2*index,2)
            channels=[((color>>shift&31)<<3)|((color>>shift&31)>>2) for shift in (0,5,10)]
            pixels.append((channels[0]<<16)|(channels[1]<<8)|channels[2])
    return pixels

def generate_portraits(rom):
    if len(rom)!=16777216 or hashlib.sha1(rom).hexdigest()!=ROM_SHA1:
        raise ValueError('Unsupported ROM identity; expected B3AE revision 0')
    output=[]
    for size,name in [(40,'third_age_portrait_patterns'),(32,'third_age_small_portrait_patterns')]:
        output.append('inline const std::array<std::vector<ThirdAgePortraitProbe>,68>& '+name+'(){\n')
        output.append('    static const std::array<std::vector<ThirdAgePortraitProbe>,68> patterns{{\n')
        for kind,count in [(6,32),(5,36)]:
            for unit_type in range(count):
                pixels=portrait_pixels(rom,kind,unit_type,size);counts=Counter(pixels)
                pairs=sorted(enumerate(pixels),key=lambda p:(counts[p[1]],p[0]))
                output.append('        {{'+','.join('{%d,0x%06xu}'%p for p in pairs)+'}},\n')
        output.append('    }};return patterns;\n}\n')
    return ''.join(output).encode()

def font_header(font):
    if hashlib.sha256(font).hexdigest()!=FONT_SHA256:
        raise ValueError('Supply the original Ringbearer TTF to preserve baseline typography; other fonts are not validated')
    # The preserved font reader expects TrueType outlines, Unicode cmap and 2048 UPEM.
    if len(font)<12 or font[:4]!=b'\x00\x01\x00\x00':raise ValueError('Expected a TrueType outline font')
    count=struct.unpack_from('>H',font,4)[0];tables={}
    if 12+16*count>len(font):raise ValueError('Truncated font directory')
    for i in range(count):
        tag,_,offset,size=struct.unpack_from('>4sIII',font,12+16*i)
        if offset+size>len(font):raise ValueError('Font table out of range')
        tables[tag]=(offset,size)
    for tag in [b'head',b'loca',b'glyf',b'cmap',b'hmtx',b'hhea']:
        if tag not in tables:raise ValueError('Required TrueType table missing')
    head,size=tables[b'head']
    if size<20 or struct.unpack_from('>H',font,head+18)[0]!=2048:
        raise ValueError('Current renderer requires a 2048-unit TrueType font')
    return ('#pragma once\n// Generated locally from a user-supplied font. Private; do not distribute.\nnamespace gbarecomp::hd_font {\ninline constexpr unsigned char ringbearer_data[]={\n'+
            '\n'.join(','.join(str(x) for x in font[i:i+24])+',' for i in range(0,len(font),24))+
            '\n};\n}\n').encode()

def generate(rom_path,font_path,output):
    rom=rom_path.read_bytes();font=font_path.read_bytes()
    portraits=generate_portraits(rom);header=font_header(font);decoder=generate_decode(rom)
    output.mkdir(parents=True,exist_ok=False)
    (output/'third_age_portrait_patterns.private.inc').write_bytes(portraits)
    (output/'third_age_ringbearer_data.h').write_bytes(header)
    (output/'third_age_native_decode.private.inc').write_bytes(decoder)
    record={'rom_sha1':hashlib.sha1(rom).hexdigest(),'font_sha256':hashlib.sha256(font).hexdigest(),
            'portrait_signatures':136,'generated_from_rom':True,'font_supplied_locally':True,
            'files':{n:hashlib.sha256(b).hexdigest() for n,b in [('third_age_portrait_patterns.private.inc',portraits),('third_age_ringbearer_data.h',header),('third_age_native_decode.private.inc',decoder)]}}
    (output/'generation.json').write_text(json.dumps(record,indent=2)+'\n')
    return record

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,required=True);p.add_argument('--font',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    try:record=generate(a.rom,a.font,a.output)
    except (OSError,ValueError,struct.error) as e:p.exit(1,str(e)+'\n')
    print(json.dumps(record,indent=2))

if __name__=='__main__':main()
