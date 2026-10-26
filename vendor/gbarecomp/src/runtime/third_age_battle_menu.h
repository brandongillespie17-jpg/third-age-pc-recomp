#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <cmath>
#include "third_age_hd_font.h"
namespace gbarecomp {
struct ThirdAgeBattleMenu {
    struct Row {std::string text;int x=8,y=8;bool disabled=false;};
    bool valid=false,paragraph=false,question=false,grid=false,pause=false,spirit_warning=false;
    int x=0,y=0,w=0,h=0,selected=-1,offset_x=40,offset_y=10,opacity=255;
    std::array<unsigned,4> scores{};
    std::vector<Row> rows;
    int help_cost=-1; // Native spirit cost; -1 for descriptions without a cost.
    std::string help_title,help,pause_title,pause_objectives,pause_score;
};
// One layout for rendering and hit testing; native game rectangles stay intact.
struct ThirdAgeMenuLayout {
    int x,y,w,h;double font=6.5;
    std::vector<ThirdAgeBattleMenu::Row> rows;
};
inline double third_age_menu_width(const std::string& text,double size){
    double w=0;unsigned prev=0;for(unsigned char c:text){w+=hd_font::kerning(prev,c)*size/2048.;if(c==1)w+=size;else{int i=c>=32&&c<=126?c-32:c==251?95:-1;if(i>=0)w+=hd_font::glyphs[i].advance*size/2048.;}prev=c;}return w;
}
inline ThirdAgeMenuLayout third_age_menu_layout(const ThirdAgeBattleMenu& p){
    ThirdAgeMenuLayout q{p.x,p.y,p.w,p.h,5.8,p.rows};
    if(!p.valid)return q;
    if(p.pause){
        if(!p.grid&&q.rows.size()==3){
            q.rows[0].x=int(std::lround((240-third_age_menu_width(q.rows[0].text,7.4))/2));
            q.rows[0].y+=3; // Give the quit question a little more space below Turn.
            q.rows[1].x=int(std::lround(80-third_age_menu_width(q.rows[1].text,7.4)/2));
            q.rows[2].x=int(std::lround(160-third_age_menu_width(q.rows[2].text,7.4)/2));
        }
        return q;
    }
    if(!p.paragraph&&!p.question){
        double width=28;for(const auto& r:p.rows)width=std::max(width,third_age_menu_width(r.text,q.font)+10);
        q.w=std::min(240,int(std::ceil(width)));q.h=5+8*int(q.rows.size());
        q.x=(240-q.w)/2;q.y=(160-q.h)/2;
        for(size_t i=0;i<q.rows.size();++i){q.rows[i].x=std::max(5,int(std::lround((q.w-third_age_menu_width(q.rows[i].text,q.font))/2)));q.rows[i].y=2+8*i;}
    }else if(p.paragraph&&!p.rows.empty()){
        q.font=5.8;unsigned lines=0;std::string line,word;
        auto flush=[&](){if(word.empty())return;if(!line.empty()&&third_age_menu_width(line+" "+word,q.font)>std::min(p.w,144)-8){++lines;line.clear();}if(!line.empty())line+=' ';line+=word;word.clear();};
        for(char c:p.rows[0].text){if(c==' '||c=='\n'){flush();if(c=='\n'){++lines;line.clear();}}else word+=c;}
        flush();if(!line.empty())++lines;
        q.w=std::min(p.w,144);
        if(lines<=1)q.w=std::min(q.w,std::max(28,int(std::ceil(third_age_menu_width(p.rows[0].text,q.font)+8))));
        q.h=int(std::ceil(6+std::max(1u,lines)*q.font*1.3));q.h=std::min(156,q.h);
        q.x=std::clamp(p.x+(p.w-q.w)/2,0,240-q.w);q.y=std::clamp(p.y,0,160-q.h);
    }else if(p.question&&p.rows.size()==3&&p.rows[1].y==p.rows[2].y){
        const double prompt=third_age_menu_width(p.rows[0].text,q.font);
        const double yes=third_age_menu_width(p.rows[1].text,q.font),no=third_age_menu_width(p.rows[2].text,q.font);
        q.w=std::clamp(int(std::ceil(std::max(prompt+10,2*std::max(yes,no)+18))),32,220);q.h=24;
        q.x=std::clamp(p.x+(p.w-q.w)/2,0,240-q.w);q.y=std::clamp(p.y+(p.h-q.h)/2,0,160-q.h);
        q.rows[0].x=std::max(5,int((q.w-prompt)/2));q.rows[0].y=3;
        q.rows[1].x=int(q.w/4.-yes/2);q.rows[2].x=int(3*q.w/4.-no/2);q.rows[1].y=q.rows[2].y=14;
    }
    return q;
}
// Coordinates here are in the centered 240x160 menu plane. Translate only
// active HD choices back to the native row; padding must not confirm a choice.
inline bool third_age_menu_pointer(const ThirdAgeBattleMenu& p,int& x,int& y){
    if(!p.valid||p.paragraph)return true;
    if(p.pause){
        if(p.grid)return true;
        if(p.rows.size()!=3||x<40||x>=200||y<144||y>=160){x=y=-32768;return false;}
        const unsigned row=x<120?1:2;
        x=p.x+p.rows[row].x+2;y=p.y+p.rows[row].y+2;return true;
    }
    const auto q=third_age_menu_layout(p);
    if(p.question){
        if(p.rows.size()!=3||p.rows[1].y!=p.rows[2].y)return true;
        if(x<q.x+2||x>=q.x+q.w-2||y<q.y+12||y>=q.y+23){x=y=-32768;return false;}
        unsigned row=x<q.x+q.w/2?1:2;x=p.x+p.rows[row].x+2;y=p.y+p.rows[row].y+2;return true;
    }
    if(x<q.x+2||x>=q.x+q.w-2||y<q.y+2||y>=q.y+2+8*int(q.rows.size())){x=y=-32768;return false;}
    const unsigned row=(y-q.y-2)/8;if(row>=p.rows.size()){x=y=-32768;return false;}
    x=p.x+p.rows[row].x+1;y=p.y+p.rows[row].y+2;return true;
}
class ThirdAgeMenuFade {
    ThirdAgeBattleMenu previous_;
    unsigned level_=0;
    static bool same(const ThirdAgeBattleMenu& a,const ThirdAgeBattleMenu& b){
        if(a.x!=b.x||a.y!=b.y||a.rows.size()!=b.rows.size()||a.paragraph!=b.paragraph||a.question!=b.question)return false;
        for(size_t i=0;i<a.rows.size();++i)if(a.rows[i].text!=b.rows[i].text)return false;return true;
    }
public:
    void clear(){previous_={};level_=0;}
    ThirdAgeBattleMenu frame(const ThirdAgeBattleMenu& next,bool dialog){
        if(next.pause){clear();return next;}
        if(!dialog){clear();return next;}
        if(next.valid&&next.opacity){
            if(!previous_.valid||!same(previous_,next))level_=0;
            level_=std::min(6u,level_+1);previous_=next;
        }else if(previous_.valid&&level_)--level_;
        else {clear();return {};}
        auto out=previous_;out.opacity=int(level_*255/6);return out;
    }
};
// Bounded, read-only versions of the game's Huffman decoder and @ substitution
// (0803e530/08015dbc). Cache only raw ROM text; substitutions remain live.
class ThirdAgeBattleText {
    uint32_t source_=0,offsets_=0,tree_=0;
    std::unordered_map<unsigned,std::string> cache_;
public:
    void clear(){source_=offsets_=tree_=0;cache_.clear();}
    template<class R> bool get(R r,unsigned id,std::string& result){
        if(id>=0x544)return false;
        auto a=r(0x03001938,4),b=r(0x0300193c,4),c=r(0x03001940,4);
        if(a!=source_||b!=offsets_||c!=tree_){clear();source_=a;offsets_=b;tree_=c;}
        auto found=cache_.find(id);
        if(found==cache_.end()){
            const uint32_t src=a+r(b+4*id,4);unsigned bit=0;std::string text;bool ended=false;
            for(unsigned i=0;i<2048;++i){unsigned node=256;
                for(unsigned depth=0;depth<32&&node>=256;++depth){if(node>511)return false;
                    unsigned choice=(r(src+bit/8,1)>>(bit%8))&1;++bit;node=r(c+4*(node-256)+2*choice,2);
                }
                if(node>=256)return false;if(!node){ended=true;break;}text+=char(node);
            }
            if(!ended)return false;found=cache_.emplace(id,std::move(text)).first;
        }
        result.clear();
        auto append=[&](unsigned ch){if(ch==0xb3)ch=251;if(ch!='\n'&&(ch<32||(ch>126&&ch!=251)))return false;result+=char(ch);return true;};
        const auto& raw=found->second;
        for(size_t i=0;i<raw.size();++i){unsigned ch=uint8_t(raw[i]);
            if(ch=='@'){
                if(++i==raw.size())return false;const unsigned slot=uint8_t(raw[i]);bool ended=false;
                for(unsigned j=0;j<64;++j){auto v=r(0x03000ba8+64*slot+j,1);if(!v){ended=true;break;}if(!append(v))return false;}
                if(!ended)return false;
            }else if(ch=='^')return false; // Unsupported native formatting retains original rendering.
            else if(!append(ch))return false;
        }
        return true;
    }
};
// Shared native vertical choices, Yes/No layouts and paged tutorial/message
// dialogs. Geometry stays native so existing mouse/keyboard controls agree.
template<class R> ThirdAgeBattleMenu third_age_battle_menu(R r,ThirdAgeBattleText& strings){
    ThirdAgeBattleMenu p;
    const auto scene=r(0x03004840,4),parent=r(0x03004860,4),world=r(0x03004898,4);
    // 04860 records the previous scene, including a submenu on return.
    // Substate 1 distinguishes the battle pause page from other objectives.
    const bool pause_origin=parent==1||parent==6||parent==19||parent==22;
    if(scene==15&&pause_origin&&(r(0x03004880,4)==15||r(0x03004880,4)>=128)&&r(0x03004844,4)==1){
        const auto state=r(0x03004850,4);if(state>2)return p;const unsigned stage=state==1?1:0;
        p.x=0;p.y=0;p.w=240;p.h=160;p.question=true;p.grid=stage==0;p.pause=true;
        if(world<0x02000000||world>0x0203fc00||!strings.get(r,0xdd,p.pause_title))return ThirdAgeBattleMenu{};
        const auto mission=r(world+0x29d,1);if(mission>=35)return ThirdAgeBattleMenu{};
        for(unsigned j=0;j<5;++j){const auto id=r(0x08054458+10*mission+2*j,2);if(!id)continue;std::string line;
            if(!strings.get(r,id,line))return ThirdAgeBattleMenu{};if(!p.pause_objectives.empty())p.pause_objectives+='\n';p.pause_objectives+=line;
        }
        const unsigned good=r(0x0300352e,1)==1?0:1,evil=good^1;
        p.scores={unsigned((r(0x030034f8+12*good,2)/10)&255),unsigned((r(0x030034f8+12*evil,2)/10)&255),unsigned(r(0x030034fc,2)/10),unsigned((r(0x0300351a,2)+1)/2)};
        p.pause_score="Good: "+std::to_string((r(0x030034f8+12*good,2)/10)&255)+"    Evil: "+std::to_string((r(0x030034f8+12*evil,2)/10)&255)+
            "    Goal: "+std::to_string(r(0x030034fc,2)/10)+"    Turn "+std::to_string((r(0x0300351a,2)+1)/2);
        p.selected=int(r(stage?0x0300484c:0x03004848,4))+(stage?1:0);
        if(p.selected<(stage?1:0)||p.selected>(stage?2:3))return ThirdAgeBattleMenu{};
        for(unsigned i=0;i<(stage?3u:4u);++i){ThirdAgeBattleMenu::Row row;
            unsigned id=stage?(i==0?0xe2:i==1?0xda:0xdb):r(0x080545bc+4*i,4);
            if(!strings.get(r,id,row.text))return ThirdAgeBattleMenu{};
            row.x=stage?(i==0?40:i==1?64:128):(i<2?44:154);
            row.y=stage?(i==0?131:146):(i%2?146:133);p.rows.push_back(std::move(row));
        }
        p.valid=true;return p;
    }
    if(scene!=2||(parent!=1&&parent!=15)||(r(0x03004880,4)!=2&&r(0x03004880,4)!=0x81)||r(0x03004850,4)>5||world<0x02000000||world>0x0203fc00)return p;
    const unsigned count=r(world+0x336,1),type=r(world+0x33b,1),flags=r(world+0x33a,1);
    if(!count||count>11||(type!=0&&type!=2&&type!=3&&type!=4))return p;
    p.x=8*r(world+0x337,1);p.y=8*r(world+0x338,1);p.w=8*r(world+0x2d0,1);p.h=8*r(world+0x2d1,1);
    if(p.w<24||p.h<16||p.x+p.w>240||p.y+p.h>160)return p;
    p.paragraph=type==3||type==4;p.question=type==2;p.selected=p.paragraph?-1:int(r(0x03004848,4));
    const unsigned page=r(world+0x33c,1);if(p.paragraph&&page>=count)return p;
    for(unsigned i=0;i<(p.paragraph?1:count);++i){
        ThirdAgeBattleMenu::Row row;const unsigned index=p.paragraph?page:i;
        const unsigned text_id=r(world+0x348+2*index,2);
        if(!strings.get(r,text_id,row.text))return ThirdAgeBattleMenu{};
        if(p.paragraph&&text_id==0x363){p.spirit_warning=true;row.text="Not enough \x01!";}
        row.disabled=r(world+0x35e + index,1)!=0;
        if(p.question){row.x+=int32_t(r(world+0x2d8+8*i,4));row.y+=int32_t(r(world+0x2dc+8*i,4));}
        else if(!p.paragraph)row.y+=10*i;
        if(row.x<0||row.y<0||row.x>=p.w||row.y>=p.h)return ThirdAgeBattleMenu{};
        p.rows.push_back(std::move(row));
    }
    if(!p.paragraph&&(p.selected<0||p.selected>=int(count)))return ThirdAgeBattleMenu{};
    if(type==0&&!(flags&2)&&p.selected>=0){
        const unsigned id=r(world+0x348+2*p.selected,2);unsigned description=0;
        // Same name/description mapping as native upgrade screen 08035188.
        if(id>=0x2cc&&id<=0x2e1)description=id+0x17;
        else if(id==0x2e2)description=0x328; // Horn of Gondor equipment skill.
        else if(id>=0x337&&id<=0x340)description=id+10;
        if(description&&strings.get(r,description,p.help)){
            p.help_title=p.rows[p.selected].text;
            // 08034394 and the ability execution path 08032560 read the
            // ushort at +4 in this 28-byte table. Upgrade prices are separate.
            if(id>=0x2cc&&id<=0x2e2)p.help_cost=int(r(0x0805af74+28*(id-0x2cc),2));
        }
    }
    if(r(0x03004850,4)==3){p.opacity=0;p.help.clear();}
    p.valid=true;return p;
}
}
