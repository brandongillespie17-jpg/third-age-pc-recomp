#pragma once
#include <array>
#include <cstdint>
#include <cstring>
namespace gbarecomp {
// Supported Third Age ROM: ARM routine 08006820. Search an open/closed
// pathfinding node list by its packed coordinate. No memory writes, RNG, IO,
// caching or cycle manipulation here. Commit registers only after validation.
struct ThirdAgeNativeNodeLookup {
    std::array<uint32_t,16> regs{};
    uint32_t cpsr=0,nodes=0;
    bool evaluate(const uint32_t* input,uint32_t flags,const uint8_t* ew,const uint8_t* iw) {
        nodes=0;
        if(input[15]!=0x08006820u || (flags&0x20) || input[1]>1)return false;
        auto read=[&](uint32_t a,unsigned n,uint32_t& v) {
            const uint8_t* p=nullptr;
            if((n==4&&(a&3))||(n==2&&(a&1)))return false;
            if(a>=0x02000000u && uint64_t(a)+n<=0x02040000u)p=ew+a-0x02000000u;
            else if(a>=0x03000000u && uint64_t(a)+n<=0x03008000u)p=iw+a-0x03000000u;
            if(!p)return false;
            v=0;for(unsigned j=0;j<n;++j)v|=uint32_t(p[j])<<(8*j);return true;
        };
        std::memcpy(regs.data(),input,64);auto& r=regs;
        r[1]=(r[1]&255)<<3;r[3]=0x03006c70;
        if(!read(r[3]+r[1],4,r[1]))return false;
        if(r[1]) {
            r[12]=0xffffffffu;
            if(!read(r[0],2,r[0]))return false;
            do {
                if(++nodes>1024 || r[1]<0x02000000u || r[1]>0x0203ffe8u)return false;
                if(!read(r[1]+8,2,r[3]) || !read(r[1],4,r[2]))return false;
                if(r[3]==r[0]){r[0]=r[1];goto done;}
                r[1]=r[2];
            }while(r[1]);
        }
        r[0]=0;
    done:
        // Every valid exit follows an equal CMP or SUBS of zero.
        cpsr=(flags&~0xf0000020u)|0x60000000u|((r[14]&1)?0x20u:0u);
        r[15]=r[14]&~1u;
        return true;
    }
};
}
