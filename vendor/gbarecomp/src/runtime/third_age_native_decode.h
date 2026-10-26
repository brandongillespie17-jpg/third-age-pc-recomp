#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
namespace gbarecomp {
// Exact-ROM native translation of the copied ARM graphics decoder. Executes
// transactionally against scratch memory, with bounded reads, writes and work.
// No IO access, device ticks, RNG changes, persistent cache, or ROM modification.
struct ThirdAgeNativeDecode {
    struct Unsupported {};
    std::array<uint8_t,262144> ew{};
    std::array<uint8_t,32768> iw{};
    std::array<uint8_t,98304> vr{};
    std::array<uint32_t,16> regs{};
    uint32_t cpsr=0,steps=0,written=0;
    struct Write {uint32_t address,value;unsigned width;};
    std::vector<Write> writes;
    const uint8_t* rom=nullptr;std::size_t rom_size=0;
    uint8_t* ram(uint32_t a,unsigned width) {
        if(a>=0x02000000u&&uint64_t(a)+width<=0x02040000u)return ew.data()+a-0x02000000u;
        if(a>=0x03000000u&&uint64_t(a)+width<=0x03008000u)return iw.data()+a-0x03000000u;
        if(a>=0x06000000u&&uint64_t(a)+width<=0x06018000u)return vr.data()+a-0x06000000u;
        throw Unsupported{};
    }
    uint32_t read(uint32_t a,unsigned width) {
        if((width==4&&(a&3))||(width==2&&(a&1)))throw Unsupported{};
        const uint8_t* p;
        if(a>=0x08000000u&&uint64_t(a)+width<=0x08000000ull+rom_size)p=rom+a-0x08000000u;
        else p=ram(a,width);
        uint32_t value=0;for(unsigned j=0;j<width;++j)value|=uint32_t(p[j])<<(8*j);return value;
    }
    void write(uint32_t a,uint32_t value,unsigned width) {
        if((width==4&&(a&3))||(width==2&&(a&1)))throw Unsupported{};
        auto* p=ram(a,width);for(unsigned j=0;j<width;++j)p[j]=uint8_t(value>>(8*j));written+=width;
        if(writes.size()>=131072)throw Unsupported{};
        writes.push_back({a,value,width});
    }
    static uint32_t shift(uint32_t value,unsigned kind,unsigned amount,bool& carry) {
        if(amount==0)return value;
        if(kind==0){carry=amount<=32?bool((value>>(32-amount))&1):false;return amount<32?value<<amount:0;}
        if(kind==1){carry=amount<=32?bool((value>>(amount-1))&1):false;return amount<32?value>>amount:0;}
        throw Unsupported{};
    }
    bool evaluate(const uint32_t* input,uint32_t flags,const uint8_t* input_ew,
                  const uint8_t* input_iw,const uint8_t* input_vr,const uint8_t* input_rom,std::size_t size) {
        if(input[15]!=0x03003b00u||(flags&0x20)||(flags&31)!=31||(input[13]&3))return false;
        uint32_t hash=2166136261u;
        for(unsigned j=0x3b00;j<0x3e2c;++j)hash=(hash^input_iw[j])*16777619u;
        if(hash!=0x76d12818u)return false;
        rom=input_rom;rom_size=size;steps=written=0;writes.clear();
        std::memcpy(regs.data(),input,64);std::memcpy(ew.data(),input_ew,ew.size());
        std::memcpy(iw.data(),input_iw,iw.size());std::memcpy(vr.data(),input_vr,vr.size());
        auto& r=regs;bool n=flags>>31,z=(flags>>30)&1,c=(flags>>29)&1,v=(flags>>28)&1;
        try {
// Generated locally from the owned ROM; never distribute translated game code.
#ifdef THIRD_AGE_PORTABLE_RELEASE
#include "third_age_runtime_decode.inc"
#else
#include "third_age_native_decode.private.inc"
#endif
        }catch(const Unsupported&){return false;}
    }
};
}
