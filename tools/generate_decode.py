"""Emit the bounded scratch-decoder fast path from the user's ROM.

This generator contains ARM instruction semantics, not the game's instruction
sequence. It deliberately accepts only the subset used by the verified routine.
Generated output must stay private alongside cartridge/BIOS generated sources.
"""
import struct

CONDITIONS=['z','!z','c','!c','n','!n','v','!v','c&&!z','!c||z','n==v','n!=v','!z&&(n==v)','z||(n!=v)','true']

def generate_decode(rom):
    base,end=0x03003b00,0x03003e2c
    data=rom[0x44c:0x44c+end-base]
    checksum=2166136261
    for byte in data:checksum=((checksum^byte)*16777619)&0xffffffff
    if len(data)!=end-base or checksum!=0x76d12818:
        raise ValueError('Graphics decoder ROM identity mismatch')
    lines=[];returns=set()
    for offset in range(0,len(data),4):
        pc=base+offset;word=struct.unpack_from('<I',data,offset)[0]
        cond=word>>28
        if cond>=len(CONDITIONS):raise ValueError('Unsupported ARM condition')
        body=''
        if word&0x0ffffff0==0x012fff10:
            if word&15!=14:raise ValueError('Only return-register BX is supported')
            body='goto return_dispatch;'
        elif word&0x0e000000==0x0a000000:
            delta=word&0xffffff
            if delta&0x800000:delta-=0x1000000
            target=pc+8+delta*4
            if not base<=target<end:raise ValueError('Branch leaves decoder')
            if word&0x01000000:
                body+=f'r[14]=0x{pc+4:08x}u;';returns.add(pc+4)
            body+=f'goto L{target:08x};'
        elif word&0x0e000000==0x08000000:
            rn=(word>>16)&15;regs=[i for i in range(16) if word&(1<<i)]
            load=bool(word&0x100000);up=bool(word&0x800000);pre=bool(word&0x1000000)
            if rn!=13 or not word&0x200000 or word&0x400000:
                raise ValueError('Unsupported block transfer')
            size=4*len(regs)
            if not load and not up and pre:
                body=f'r[{rn}]-={size};'+''.join(f'write(r[{rn}]+{4*i},r[{reg}],4);' for i,reg in enumerate(regs))
            elif load and up and not pre:
                body=''.join(f'r[{reg}]=read(r[{rn}]+{4*i},4);' for i,reg in enumerate(regs))+f'r[{rn}]+={size};'
            else:raise ValueError('Unsupported block-transfer addressing')
        elif word&0x0c000000==0x04000000 or word&0x0e000090==0x00000090:
            half=word&0x0c000000!=0x04000000
            rn=(word>>16)&15;rd=(word>>12)&15
            pre=bool(word&0x1000000);up=bool(word&0x800000);writeback=bool(word&0x200000);load=bool(word&0x100000)
            if not up:raise ValueError('Unsupported descending transfer')
            if half:
                if (word>>5)&3!=1:raise ValueError('Unsupported signed/multiply encoding')
                width=2
                operand=str(((word>>4)&0xf0)|(word&15)) if word&0x400000 else f'r[{word&15}]'
            else:
                width=1 if word&0x400000 else 4
                if word&0x2000000:
                    if word&0xff0:raise ValueError('Unsupported shifted memory offset')
                    operand=f'r[{word&15}]'
                else:operand=str(word&0xfff)
            address=f'r[{rn}]+{operand}' if pre else f'r[{rn}]'
            body=f'const uint32_t address={address};const uint32_t next=r[{rn}]+{operand};'
            body+=f'r[{rd}]=read(address,{width});' if load else f'write(address,r[{rd}],{width});'
            if not pre or writeback:body+=f'r[{rn}]=next;'
        elif word&0x0c000000==0:
            op=(word>>21)&15;setflags=bool(word&0x100000);rn=(word>>16)&15;rd=(word>>12)&15
            carry=''
            if word&0x2000000:
                imm=word&255;rot=((word>>8)&15)*2
                value=((imm>>rot)|(imm<<(32-rot)))&0xffffffff if rot else imm
                operand=f'0x{value:08x}u'
                if rot:carry='carry='+('true' if value>>31 else 'false')+';'
            else:
                rm=word&15;kind=(word>>5)&3
                if kind>1:raise ValueError('Unsupported shift kind')
                amount=f'(r[{(word>>8)&15}]&255u)' if word&16 else str((word>>7)&31)
                operand=f'shift(r[{rm}],{kind},{amount},carry)'
            body=f'bool carry=c;{carry}const uint32_t a=r[{rn}],b={operand};uint32_t value;'
            semantics={0:'value=a&b;',1:'value=a^b;',2:'value=a-b;',3:'value=b-a;',
                       4:'const uint64_t sum=uint64_t(a)+b;value=uint32_t(sum);',
                       5:'const uint64_t sum=uint64_t(a)+b+uint32_t(c);value=uint32_t(sum);',
                       8:'value=a&b;',10:'value=a-b;',12:'value=a|b;',13:'value=b;'}
            if op not in semantics:raise ValueError('Unsupported arithmetic opcode')
            body+=semantics[op]
            if setflags or op in (8,10):
                if op in (2,10):body+='c=a>=b;v=bool(((a^b)&(a^value))>>31);'
                elif op in (4,5):body+='c=bool(sum>>32);v=bool((~(a^b)&(a^value))>>31);'
                elif op==3:raise ValueError('Unsupported flagged reverse subtract')
                else:body+='c=carry;'
                body+='n=bool(value>>31);z=value==0;'
            if op not in (8,10):body+=f'r[{rd}]=value;'
        else:raise ValueError('Unsupported ARM encoding')
        lines.append(f'L{pc:08x}: if(++steps>2000000)throw Unsupported{{}}; if({CONDITIONS[cond]}){{{body}}}\n')
    lines.append('return_dispatch:\n            switch(r[14]&~1u) {\n')
    for target in sorted(returns):lines.append(f'            case 0x{target:08x}:goto L{target:08x};\n')
    lines.append('''            default:
                if(r[14]!=input[14])throw Unsupported{};
                r[15]=r[14]&~1u;
                cpsr=(flags&~0xf0000020u)|(uint32_t(n)<<31)|(uint32_t(z)<<30)|
                    (uint32_t(c)<<29)|(uint32_t(v)<<28)|((r[14]&1u)?0x20u:0u);
                return true;
            }
''')
    return ''.join(lines).encode()
