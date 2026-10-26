#pragma once
// Resource decoding only. All pixels come from the selected ROM at runtime.
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>
namespace gbarecomp {
struct ThirdAgePortraitProbe {uint16_t offset;uint32_t rgb;};
using ThirdAgePortraitPatterns=std::array<std::vector<ThirdAgePortraitProbe>,68>;
inline ThirdAgePortraitPatterns runtime_portraits40, runtime_portraits32;
inline const ThirdAgePortraitPatterns& third_age_portrait_patterns(){return runtime_portraits40;}
inline const ThirdAgePortraitPatterns& third_age_small_portrait_patterns(){return runtime_portraits32;}
inline std::vector<ThirdAgePortraitProbe> decode_portrait(const uint8_t* rom,size_t length,unsigned kind,unsigned type,unsigned size){
    auto read=[&](uint64_t address,unsigned count)->uint32_t{
        if(!rom||address<0x08000000ull||address+count>0x08000000ull+length||count>4)
            throw std::runtime_error("Portrait resource outside ROM");
        uint32_t value=0;for(unsigned j=0;j<count;++j)value|=uint32_t(rom[address-0x08000000ull+j])<<(8*j);return value;
    };
    if((kind!=5&&kind!=6)||type>=(kind==5?36u:32u)||(size!=32&&size!=40))
        throw std::runtime_error("Invalid portrait request");
    const uint64_t table=read(0x08a0c188ull+4*(kind-5),4);
    const uint64_t src=read(table+0xe8*type+(size==40?0xe4:0xe0),4);
    const unsigned flags=read(src,1),extra=read(src+1,1),tiles=size/8;
    if((flags!=0x1a&&flags!=0x3a)||(extra&1)||read(src+34,2)!=tiles||read(src+36,2)!=tiles)
        throw std::runtime_error("Unsupported portrait resource layout");
    const uint64_t stored_at=((src+38+tiles*tiles*2+2+3)/4)*4-2;
    const unsigned stored=read(stored_at,2);uint64_t at=stored_at+2;
    std::vector<uint8_t> art;
    if(flags==0x1a){
        if(!stored||stored>tiles*tiles*32||stored%32)throw std::runtime_error("Invalid raw tile count");
        for(unsigned j=0;j<stored;++j)art.push_back(uint8_t(read(at+j,1)));
    }else{
        const uint64_t end=at+stored;const unsigned header=read(at,4),decoded=header>>8;at+=4;
        if((header&255)!=0x30||!decoded||decoded>tiles*tiles*32||decoded%32||stored<4)
            throw std::runtime_error("Invalid portrait RLE header");
        while(art.size()<decoded){
            if(at>=end)throw std::runtime_error("Truncated RLE control");
            unsigned control=read(at++,1),count=(control&127)+((control&128)?3:1);
            if(art.size()+count>decoded)throw std::runtime_error("RLE output overrun");
            if(control&128){if(at>=end)throw std::runtime_error("Truncated RLE repeat");art.insert(art.end(),count,uint8_t(read(at++,1)));}
            else{if(at+count>end)throw std::runtime_error("Truncated RLE literal");while(count--)art.push_back(uint8_t(read(at++,1)));}
        }
    }
    std::vector<ThirdAgePortraitProbe> pixels;pixels.reserve(size*size);
    std::unordered_map<uint32_t,unsigned> counts;
    for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x){
        unsigned entry=read(src+38+2*((y/8)*tiles+x/8),2),tile=entry&1023;
        if(tile>=art.size()/32)throw std::runtime_error("Portrait tile out of range");
        unsigned px=(entry&1024)?7-x%8:x%8,py=(entry&2048)?7-y%8:y%8;
        unsigned index=(art[tile*32+py*4+px/2]>>(4*(px%2)))&15,color=read(src+2+2*index,2);
        auto channel=[&](unsigned shift){unsigned c=(color>>shift)&31;return (c<<3)|(c>>2);};
        uint32_t rgb=(channel(0)<<16)|(channel(5)<<8)|channel(10);
        pixels.push_back({uint16_t(y*size+x),rgb});++counts[rgb];
    }
    std::sort(pixels.begin(),pixels.end(),[&](const auto& a,const auto& b){
        return counts[a.rgb]!=counts[b.rgb]?counts[a.rgb]<counts[b.rgb]:a.offset<b.offset;
    });return pixels;
}
inline void initialize_runtime_portraits(const uint8_t* rom,size_t size){
    // Publish both tables only after every bounded resource decode succeeds.
    ThirdAgePortraitPatterns p40,p32;unsigned id=0;
    for(unsigned kind:{6u,5u})for(unsigned type=0;type<(kind==6?32u:36u);++type,++id){
        p40[id]=decode_portrait(rom,size,kind,type,40);p32[id]=decode_portrait(rom,size,kind,type,32);
    }
    runtime_portraits40=std::move(p40);runtime_portraits32=std::move(p32);
}
}
