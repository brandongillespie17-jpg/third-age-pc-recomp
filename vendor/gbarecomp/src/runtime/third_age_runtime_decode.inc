// Bounded ARM semantics for the existing transactional graphics fast path.
// Instructions are read from the user's validated RAM copy, never embedded.
constexpr uint32_t base=0x03003b00u,end=0x03003e2cu;
std::array<uint32_t,(end-base)/4> code{};
std::vector<uint32_t> returns;
for(unsigned j=0;j<code.size();++j){
    code[j]=read(base+4*j,4);
    if((code[j]&0x0f000000u)==0x0b000000u)returns.push_back(base+4*j+4);
}
auto condition=[&](unsigned cond){switch(cond){
    case 0:return z;case 1:return !z;case 2:return c;case 3:return !c;
    case 4:return n;case 5:return !n;case 6:return v;case 7:return !v;
    case 8:return c&&!z;case 9:return !c||z;case 10:return n==v;
    case 11:return n!=v;case 12:return !z&&(n==v);case 13:return z||(n!=v);
    case 14:return true;default:throw Unsupported{};
}};
uint32_t pc=base;
for(;;){
    bool returning=pc==end;
    if(!returning){
        if(pc<base||pc>=end||(pc&3)||++steps>2000000)throw Unsupported{};
        const uint32_t word=code[(pc-base)/4];uint32_t next=pc+4;
        if(!condition(word>>28)){pc=next;continue;}
        if((word&0x0ffffff0u)==0x012fff10u){
            if((word&15)!=14)throw Unsupported{};returning=true;
        }else if((word&0x0e000000u)==0x0a000000u){
            int32_t delta=int32_t(word&0xffffff);if(delta&0x800000)delta-=0x1000000;
            const int64_t target=int64_t(pc)+8+int64_t(delta)*4;
            if(target<base||target>=end)throw Unsupported{};
            if(word&0x01000000u)r[14]=pc+4;next=uint32_t(target);
        }else if((word&0x0e000000u)==0x08000000u){
            unsigned rn=(word>>16)&15,count=0;for(unsigned j=0;j<16;++j)count+=(word>>j)&1;
            bool load=word&0x100000,up=word&0x800000,pre=word&0x1000000;
            if(rn!=13||!(word&0x200000)||(word&0x400000))throw Unsupported{};
            unsigned at=0;
            if(!load&&!up&&pre){r[rn]-=4*count;for(unsigned j=0;j<16;++j)if(word&(1u<<j))write(r[rn]+4*at++,r[j],4);}
            else if(load&&up&&!pre){for(unsigned j=0;j<16;++j)if(word&(1u<<j))r[j]=read(r[rn]+4*at++,4);r[rn]+=4*count;}
            else throw Unsupported{};
        }else if((word&0x0c000000u)==0x04000000u||(word&0x0e000090u)==0x00000090u){
            bool half=(word&0x0c000000u)!=0x04000000u;
            unsigned rn=(word>>16)&15,rd=(word>>12)&15,width,operand;
            bool pre=word&0x1000000,up=word&0x800000,writeback=word&0x200000,load=word&0x100000;
            if(!up)throw Unsupported{};
            if(half){if(((word>>5)&3)!=1)throw Unsupported{};width=2;operand=(word&0x400000)?((word>>4)&0xf0)|(word&15):r[word&15];}
            else{width=(word&0x400000)?1:4;if(word&0x2000000){if(word&0xff0)throw Unsupported{};operand=r[word&15];}else operand=word&0xfff;}
            const uint32_t address=pre?r[rn]+operand:r[rn],updated=r[rn]+operand;
            if(load)r[rd]=read(address,width);else write(address,r[rd],width);
            if(!pre||writeback)r[rn]=updated;
        }else if((word&0x0c000000u)==0){
            unsigned op=(word>>21)&15,rn=(word>>16)&15,rd=(word>>12)&15;bool setflags=word&0x100000,carry=c;
            uint32_t operand;
            if(word&0x2000000){unsigned imm=word&255,rot=((word>>8)&15)*2;operand=rot?(imm>>rot)|(imm<<(32-rot)):imm;if(rot)carry=operand>>31;}
            else{unsigned kind=(word>>5)&3;if(kind>1)throw Unsupported{};
                unsigned amount=(word&16)?r[(word>>8)&15]&255:(word>>7)&31;operand=shift(r[word&15],kind,amount,carry);}
            const uint32_t a=r[rn],b=operand;uint32_t value;uint64_t sum=0;
            switch(op){case 0:case 8:value=a&b;break;case 1:value=a^b;break;case 2:case 10:value=a-b;break;
                case 3:value=b-a;break;case 4:sum=uint64_t(a)+b;value=uint32_t(sum);break;
                case 5:sum=uint64_t(a)+b+uint32_t(c);value=uint32_t(sum);break;
                case 12:value=a|b;break;case 13:value=b;break;default:throw Unsupported{};}
            if(setflags||op==8||op==10){
                if(op==2||op==10){c=a>=b;v=bool(((a^b)&(a^value))>>31);}
                else if(op==4||op==5){c=bool(sum>>32);v=bool((~(a^b)&(a^value))>>31);}
                else if(op==3)throw Unsupported{};else c=carry;
                n=bool(value>>31);z=value==0;
            }
            if(op!=8&&op!=10)r[rd]=value;
        }else throw Unsupported{};
        pc=next;
    }
    if(returning){
        uint32_t target=r[14]&~1u;
        if(std::find(returns.begin(),returns.end(),target)!=returns.end()){pc=target;continue;}
        if(r[14]!=input[14])throw Unsupported{};
        r[15]=target;cpsr=(flags&~0xf0000020u)|(uint32_t(n)<<31)|(uint32_t(z)<<30)|
            (uint32_t(c)<<29)|(uint32_t(v)<<28)|((r[14]&1u)?0x20u:0u);
        return true;
    }
}
