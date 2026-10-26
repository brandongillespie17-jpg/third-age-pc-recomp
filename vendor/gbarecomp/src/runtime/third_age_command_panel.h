#pragma once
#include "third_age_unit_panel.h"
#include <vector>
namespace gbarecomp {
enum class ThirdAgeTurnStyle { Normal,FreeMove,Rally,Disorder,Onslaught };
struct ThirdAgeCommandPanel {
    bool valid=false,has_portrait=false;
    uint32_t address=0,portrait_source=0;
    unsigned player=0,flank=0;
    int spirit=0,max_spirit=0,turns=0;
    ThirdAgeTurnStyle turn_style=ThirdAgeTurnStyle::Normal;
    std::string name;
    std::array<uint32_t,40*40> portrait{};
};
// Exact-ROM, read-only counterparts of the native bottom card's 08019c20,
// 08019d74 and 08019f10. Turn points are per flank; spirit is per player.
template<class R> ThirdAgeCommandPanel third_age_command_panel(R r){
    ThirdAgeCommandPanel p;
    p.player=r(0x03003536,1);if(p.player>1)return p;
    const auto world=r(0x03004898,4);
    if(world<0x02000000||world>0x0203fb00)return p;
    p.flank=r(0x03003a9c+16*p.player,1);if(p.flank>2)return p;
    p.turns=std::max(0,int(int16_t(r(0x03003434+0x5c*p.flank,2))));
    // Free move is consumed by 080097f0 (and restored by native undo).
    // Onslaught's flank bit is set by 080092a4 and cleared at turn end.
    // Rally/disorder alter the turn's point pool immediately. Their native
    // event IDs (0804ece8) remain the turn's annotation after the animation.
    // Phase 5 means turn-start rolling is complete; earlier phases must not
    // reuse the previous event. Early/tutorial turns cannot roll these events.
    const auto faction=r(0x03003531,1);
    if(r(0x0300352a,1)==1)p.turn_style=ThirdAgeTurnStyle::FreeMove;
    else if(faction<=1&&(r(0x03003407+0x5c*p.flank+12*faction,1)&2))
        p.turn_style=ThirdAgeTurnStyle::Onslaught;
    else if(r(world+0x177+0xdc*p.player,1)==5&&r(0x0300351a,2)>7&&
            r(0x03004820,4)!=1&&r(0x03004820,4)!=2&&r(0x03003530,1)==p.flank){
        const auto event=r(world+0x176+0xdc*p.player,1);
        if(event==1&&p.turns>0)p.turn_style=ThirdAgeTurnStyle::Rally;
        else if(event==4&&p.turns==0)p.turn_style=ThirdAgeTurnStyle::Disorder;
    }
    p.spirit=int16_t(r(0x03003522+2*p.player,2));
    p.max_spirit=int16_t(r(0x03003526+2*p.player,2));
    if(p.spirit<0||p.max_spirit<0)return p;
    p.address=r(world+0xa0+0xdc*p.player,4);
    if(p.address){
        if(p.address<0x02000000||p.address>0x0203fec0)return p;
        const unsigned kind=r(p.address+8,1),type=r(p.address+10,2);
        if(kind<5||kind>6||type>=(kind==5?36u:32u))return p;
        p.portrait_source=r(r(0x08a0c188+4*(kind-5),4)+0xe8*type+0xe4,4);
        const auto unit=third_age_unit_panel(r,p.address);
        p.name=unit.valid?unit.name:"Commander";
    }else{
        const auto side=r(0x03003531,1);if(side>1)return p;
        p.portrait_source=side?0x08f79bdc:0x08f79f14;
        p.name=side?"Forces of Good":"Forces of Evil";
    }
    p.valid=true;return p;
}
// Original 40x40 portrait asset: 16-color palette, tilemap, raw or BIOS RLE
// tile bytes. Bounded output, no guest decompression or renderer dependence.
template<class R> bool third_age_command_portrait(R r,uint32_t src,std::array<uint32_t,1600>& out){
    if(src<0x08000000||src>0x09ffff00)return false;
    const unsigned flags=r(src,1),extra=r(src+1,1);
    if((flags!=0x1a&&flags!=0x3a)||(extra&1))return false;
    if(r(src+34,2)!=5||r(src+36,2)!=5)return false;
    uint32_t at=src+90;const unsigned stored=r(at,2);at+=2;
    std::vector<uint8_t> art;
    if(flags==0x1a){if(!stored||stored>800||stored%32)return false;for(unsigned i=0;i<stored;++i)art.push_back(r(at+i,1));}
    else{
        const uint32_t end=at+stored;const auto header=r(at,4);at+=4;
        const unsigned size=header>>8;
        if((header&255)!=0x30||!size||size>800||size%32||stored<4||stored>1604)return false;
        while(art.size()<size){
            if(at>=end)return false;
            const unsigned control=r(at++,1),count=(control&127)+((control&128)?3:1);
            if(art.size()+count>size)return false;
            if(control&128){if(at>=end)return false;const auto color=r(at++,1);art.insert(art.end(),count,uint8_t(color));}
            else{if(at+count>end)return false;for(unsigned j=0;j<count;++j)art.push_back(r(at++,1));}
        }
    }
    for(unsigned y=0;y<40;++y)for(unsigned x=0;x<40;++x){
        const auto entry=r(src+38+2*((y/8)*5+x/8),2);const unsigned tile=entry&1023;
        if(tile>=art.size()/32)return false;
        const unsigned px=(entry&1024)?7-x%8:x%8,py=(entry&2048)?7-y%8:y%8;
        const unsigned index=(art[tile*32+py*4+px/2]>>(4*(px%2)))&15;
        const unsigned color=r(src+2+2*index,2);
        auto rgb=[](unsigned c){return (c<<3)|(c>>2);};
        out[y*40+x]=index?0xff000000u|(rgb(color&31)<<16)|(rgb((color>>5)&31)<<8)|rgb((color>>10)&31):0;
    }
    return true;
}
}
