#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <cstring>
#include "../armv4t/interpreter.h"
#include "../armv4t/arm_decode.h"
#include "../armv4t/thumb_decode.h"
namespace gbarecomp {
// Batch the exact ROM path-search request without advancing devices inside its
// pure RAM computation. Unsupported execution discards scratch writes and falls
// back to the original scheduler. No algorithm, tie-break, RNG or path cache change.
class ThirdAgeFastPath : public armv4t::Bus {
    struct Unsupported {};
    const uint8_t* rom_=nullptr;std::size_t size_=0;
    struct Cached {uint32_t tag=0;armv4t::Instr insn{};};
    std::array<Cached,4096> cache_{};
    uint8_t* pointer(uint32_t a,unsigned n) {
        if((n==4&&(a&3))||(n==2&&(a&1)))throw Unsupported{};
        if(a>=0x02000000u&&uint64_t(a)+n<=0x02040000u)return ew.data()+a-0x02000000u;
        if(a>=0x03000000u&&uint64_t(a)+n<=0x03008000u)return iw.data()+a-0x03000000u;
        throw Unsupported{};
    }
    const uint8_t* readable(uint32_t a,unsigned n) {
        if(a>=0x08000000u&&uint64_t(a)+n<=0x08000000ull+size_)return rom_+a-0x08000000u;
        return pointer(a,n);
    }
    uint32_t read(uint32_t a,unsigned n){const auto* p=readable(a,n);uint32_t v=0;for(unsigned j=0;j<n;++j)v|=uint32_t(p[j])<<(j*8);return v;}
    void write(uint32_t a,uint32_t v,unsigned n){
        if(writes.size()>=262144)throw Unsupported{};
        auto* p=pointer(a,n);for(unsigned j=0;j<n;++j)p[j]=uint8_t(v>>(j*8));writes.push_back({a,v,n});written+=n;
    }
    uint8_t read8(uint32_t a)override{return uint8_t(read(a,1));}
    uint16_t read16(uint32_t a)override{return uint16_t(read(a,2));}
    uint32_t read32(uint32_t a)override{return read(a,4);}
    void write8(uint32_t a,uint8_t v)override{write(a,v,1);}
    void write16(uint32_t a,uint16_t v)override{write(a,v,2);}
    void write32(uint32_t a,uint32_t v)override{write(a,v,4);}
public:
    std::array<uint8_t,262144> ew{};
    std::array<uint8_t,32768> iw{};
    std::array<uint32_t,16> regs{};
    struct Write{uint32_t address,value;unsigned width;};
    std::vector<Write> writes;
    uint32_t cpsr=0,steps=0,written=0;
    bool evaluate(const uint32_t* input,uint32_t flags,const uint8_t* input_ew,
                  const uint8_t* input_iw,const uint8_t* rom,std::size_t size) {
        if(input[15]!=0x0802d51cu || !(flags&32) || (flags&31)!=31 || (input[13]&3))return false;
        if(rom_!=rom||size_!=size){for(auto& e:cache_)e.tag=0;rom_=rom;size_=size;}
        std::memcpy(ew.data(),input_ew,ew.size());std::memcpy(iw.data(),input_iw,iw.size());
        writes.clear();steps=written=0;
        armv4t::CPUState cpu{};std::memcpy(cpu.R,input,64);
        cpu.cpsr.n=(flags>>31)&1;cpu.cpsr.z=(flags>>30)&1;cpu.cpsr.c=(flags>>29)&1;cpu.cpsr.v=(flags>>28)&1;
        cpu.cpsr.i=(flags>>7)&1;cpu.cpsr.f=(flags>>6)&1;cpu.cpsr.t=true;cpu.cpsr.mode=31;cpu.thumb=true;
        const uint32_t target=input[14]&~1u;
        try {
            do {
                if(++steps>2000000)throw Unsupported{};
                const auto pc=cpu.R[15];
                if(pc<0x08000000u || uint64_t(pc)+4>0x08000000ull+size_ || (pc&(cpu.thumb?1:3)))throw Unsupported{};
                const uint32_t tag=pc|uint32_t(cpu.thumb);auto& entry=cache_[(pc>>1)&4095];
                if(entry.tag!=tag){
                    entry.insn=cpu.thumb?armv4t::ThumbDecoder::decode(read16(pc),pc):armv4t::ArmDecoder::decode(read32(pc),pc);
                    entry.tag=tag;
                }
                const auto result=armv4t::Interpreter::step(cpu,*this,entry.insn);
                if((result!=armv4t::Interpreter::Result::Normal&&result!=armv4t::Interpreter::Result::Branched)||
                   cpu.cpsr.mode!=31||cpu.cpsr.i!=bool(flags&128)||cpu.cpsr.f!=bool(flags&64))throw Unsupported{};
            }while(cpu.R[15]!=target);
            std::memcpy(regs.data(),cpu.R,64);
            cpsr=(flags&~0xf0000020u)|(uint32_t(cpu.cpsr.n)<<31)|(uint32_t(cpu.cpsr.z)<<30)|
                (uint32_t(cpu.cpsr.c)<<29)|(uint32_t(cpu.cpsr.v)<<28)|(uint32_t(cpu.cpsr.t)<<5);
            return true;
        }catch(const Unsupported&){return false;}
    }
};
}
