#pragma once
#include <array>
#include <string>
#include <cstdint>
#include <algorithm>
namespace gbarecomp {
// Presentation data only. ROM strings/art are decoded from the player's game;
// none are bundled. Invalid/unknown states retain the original HUD.
struct ThirdAgeUnitPanel {
    bool terrain=false;
    int defense=0;
    bool valid=false,erase_native=false,has_portrait=false;
    uint32_t address=0;
    unsigned kind=0,type=0;
    int hp=0,max_hp=0,side=0;
    int attack=0,range=0,movement=0;
    std::string name;
    std::array<uint8_t,40*40*3> underlay{};
    std::array<uint32_t,32*32> portrait{};
};
// R must be a bounded, side-effect-free reader which throws on invalid access.
// Matches 080198a0 (HUD identity), 08040a10/0801d22c (health),
// 08040fac (hero save index), 08041ed0 (name ID), 0801af28 (text).
template<class R> ThirdAgeUnitPanel third_age_unit_panel(R r,uint32_t address=0) {
    ThirdAgeUnitPanel p;
    if(!address&&(r(0x03004840,4)!=1||r(0x03004880,4)!=1))return p;
    const unsigned player=r(0x03003536,1);
    if(player>1||(!address&&player!=r(0x030033e0+0x156,1)))return p;
    const uint32_t world=r(0x03004898,4);
    if(world<0x02000000||world>0x0203fb00)return p;
    // Pending native card rebuild: do not pair a new name with an old portrait.
    if(!address&&(r(world+0x168+0xdc*player,4)&0x604))return p;
    p.address=address?address:r(world+0x9c+0xdc*player,4);
    if(p.address<0x02000000||p.address>0x0203fec0)return p;
    const unsigned kind=r(p.address+8,1),type=r(p.address+10,2);
    if(kind<5||kind>6||type>=(kind==5?36u:32u))return p;
    if(!address&&(!(r(p.address+12,4)&0x10)||(r(p.address+12,4)&2)))return p;
    p.kind=kind;p.type=type;
    p.side=kind-5;
    auto hero_index=[&](){
        if(type==r(0x030048f6+8*p.side,1))return 0;
        const uint32_t table=r(0x08a0a694+4*p.side,4);
        for(unsigned i=0;i<8;++i)if(r(table+i,1)==type)return int(i+1);
        return -1;
    };
    const uint32_t stat=r(0x08a0c230+4*p.side,4)+16*type;
    const int base=r(stat,1);
    int upgrade=0,bonus=0;
    uint32_t upgrades=0;
    const unsigned hero=r(stat+12,1);
    if(hero!=255){
        if(hero>=32)return p;
        const uint32_t descriptor=r(0x08a0c238+4*p.side,4)+8*hero;
        if(!(r(descriptor+3,1)&1)){
            const int index=hero_index();if(index<0)return p;
            upgrades=r(0x03007590+4*p.side,4)+40*index;
            upgrade=int8_t(r(upgrades+2,2));
        }
    }
    const uint32_t ai=r(0x03007434,4);
    if(ai<0x02000000||ai>0x0203fa80)return p;
    if(r(ai+0x576,1)==1&&r(ai+0x579,1)==unsigned(p.side)&&r(0x03004820,4)!=1){
        const unsigned difficulty=r(world+0x46d,1);if(difficulty>2)return p;
        bonus=base*int(r(0x0805c73a+2*difficulty,2))/100;
    }
    p.hp=std::max(0,int(int8_t(r(p.address+0x87,1))));
    p.max_hp=int16_t(base+upgrade+bonus);
    if(p.max_hp<=0||p.max_hp>127||p.hp>p.max_hp)return p;
    // The native information card (08019af0) displays Attack, Movement and
    // Range using 080405f0(actor,0,1), 08040470 and 08040538 respectively.
    // Read the same live modifiers without executing code or writing guest RAM.
    auto up=[&](unsigned i){return upgrades?int(int8_t(r(upgrades+2*i,2))):0;};
    unsigned flank=0;
    if(r(p.address+0x104,2)!=65535){
        const unsigned tile=r(p.address+0x104,1);
        while(flank<3&&tile>r(0x030033e0+0x50+0x5c*flank,1))++flank;
    }else{
        const uint32_t x=r(p.address+24,4);
        while(flank<3&&x>(r(0x030033e0+0x44+0x5c*flank,4)<<16))++flank;
    }
    const uint32_t flank_stats=0x030033e0+0x5c*flank+12*p.side;
    const bool inspired=(r(flank_stats+0x27,1)&2)!=0;
    int attack=int16_t(int(r(stat+9,1))+int8_t(r(p.address+0x8b,1)));
    attack=int16_t(attack+int8_t(up(3)+(r(p.address+0x7a,1)==0?up(11):0)));
    if(r(ai+0x576,1)==1&&r(ai+0x579,1)==unsigned(p.side)){
        const unsigned difficulty=r(world+0x46d,1);if(difficulty>2)return p;
        attack=int16_t(attack+r(0x0805c746+2*difficulty,2));
    }
    p.attack=std::max(0,int(int16_t(attack+(inspired?2:0))));
    int move=r(stat+8,1);
    if(move&&r(p.address+0x7c,1)==0){
        move=std::max(0,int(int16_t(move+int8_t(r(flank_stats+0x1c,1)))));
        move=int16_t(move+r(p.address+0x82,1)+(inspired?2:0));
        if((r(p.address+0x68,4)&0x4000)&&move>0)move=1;
        p.movement=std::max(0,move);
    }
    int reach=r(stat+10,1);
    if(reach>1)reach=int16_t(reach+int8_t(r(flank_stats+0x1f,1))+int8_t(r(p.address+0x83,1))+up(5));
    const unsigned active=r(0x030033e0+0x156,1);
    if(active<2){const uint32_t cursor=r(0x03003a90+16*active,4);
        if(cursor>=0x02000000&&cursor<=0x0203fec0&&r(cursor+0x4d,1)==0x20)reach=7;
    }
    p.range=std::max(0,reach);
    const unsigned id=(p.side?0x15e:0x17e)+type;
    const uint32_t source=r(0x03001938,4)+r(r(0x0300193c,4)+4*id,4),tree=r(0x03001940,4);
    unsigned bit=0;
    for(unsigned i=0;i<64;++i){
        unsigned node=256;
        for(unsigned depth=0;depth<32&&node>=256;++depth){
            if(node>511)return p;
            const unsigned choice=(r(source+bit/8,1)>>(bit%8))&1;++bit;
            node=r(tree+4*(node-256)+2*choice,2);
        }
        if(node>=256)return p;
        if(node==0){p.valid=!p.name.empty();return p;}
        // The English ROM font uses 0xb3 for û in Mûmakil. Other unknown
        // encodings fall back, rather than printing misleading characters.
        if(node==0xb3)node=251;
        if((node<32||node>126)&&node!=251)return p;
        p.name+=char(node);
    }
    return p;
}
// Same tile lookup as 0804658c/0802dde0 and rule table as 0802dda8/
// 0804112c. Cursor coordinates are pixels; map row zero begins at y=32.
// Read-only: this does not execute guest functions or change pathfinding.
template<class Reader> ThirdAgeUnitPanel third_age_terrain_panel(Reader r,uint32_t cursor){
    ThirdAgeUnitPanel p;
    auto ram=[](uint32_t a,unsigned n){return a>=0x02000000u&&uint64_t(a)+n<=0x02040000ull;};
    if(!ram(cursor,32))return p;
    const uint32_t grid=r(0x03004818,4);
    if(!ram(grid,28))return p;
    const int x=int32_t(r(cursor+24,4))/65536,y=int32_t(r(cursor+28,4))/65536-32;
    const unsigned step=r(grid+26,1),count=r(grid+24,2);
    if(!step||!count||count>256||x<0||y<0)return p;
    const unsigned tx=unsigned(x)/step,ty=unsigned(y)/step;
    if(tx>=count||ty>=count||tx>=r(grid+12,4)||ty>=r(grid+16,4))return p;
    const uint32_t columns=r(grid,4);
    if(!ram(columns,4*count))return p;
    const uint32_t column=r(columns+4*tx,4);
    if(!ram(column,22*count))return p;
    const unsigned type=r(column+22*ty+20,1);
    static constexpr const char* names[]={"Clear","Rough","Woods","Mud","Water","Grass","Fort","Ruins","Blocked","Ladder"};
    if(type>=10)return p;
    p.terrain=true;p.name=names[type];p.type=type;
    p.movement=r(0x080547ac+8*type,1);
    p.defense=-int16_t(r(0x080547b0+8*type,2));
    if((p.movement!=2&&p.movement!=3&&p.movement!=200)||p.defense<-1||p.defense>3)return {};
    p.valid=true;return p;
}

}
