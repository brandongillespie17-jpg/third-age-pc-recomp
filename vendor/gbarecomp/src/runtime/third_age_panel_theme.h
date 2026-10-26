#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace gbarecomp {
struct ThirdAgePanelTheme {
    enum Part {BackgroundTop,BackgroundBottom,BorderLight,BorderDark,PortraitBorder,NameText,HealthText,StatText,SwordIcon,BowIcon,FootIcon,HealthFill,LowHealthFill,HealthTrack,SpiritText,TurnText,SpiritIcon,TurnIcon,FreeMoveOutline,RallyOutline,DisorderOutline,OnslaughtOutline,Count};
    inline static constexpr std::array<uint32_t,Count> defaults={0x19251f,0x121e18,0xb39b63,0x786943,0x938254,0xf0deb4,0xdeded0,0xdeded0,0xf0deb4,0xf0deb4,0xf0deb4,0x9caf83,0xbf775b,0x070d0b,0x87cbd5,0xf0deb4,0x87cbd5,0xf0deb4,0x338b48,0xb98af0,0xef6565,0xf0a24b};
    inline static constexpr const char* keys[Count]={"background_top","background_bottom","border_light","border_dark","portrait_border","name_text","health_text","stat_text","sword_icon","bow_icon","foot_icon","health_fill","low_health_fill","health_track","spirit_text","turn_text","spirit_icon","turn_icon","free_move_outline","rally_outline","disorder_outline","onslaught_outline"};
    inline static constexpr const char* labels[Count]={"BACKGROUND TOP","BACKGROUND BOTTOM","BORDER HIGHLIGHT","BORDER SHADOW","PORTRAIT BORDER","UNIT NAME","HEALTH TEXT","STAT NUMBERS","SWORD ICON","BOW ICON","FOOT ICON","HEALTH FILL","LOW HEALTH FILL","EMPTY HEALTH BAR","SPIRIT POINTS","TURN POINTS","SPIRIT SYMBOL","TURN SYMBOL","FREE MOVE OUTLINE","RALLY OUTLINE","DISORDER OUTLINE","ONSLAUGHT OUTLINE"};
    std::array<uint32_t,Count> colors=defaults;
    uint32_t operator[](Part p)const{return 0xff000000u|colors[p];}
    bool operator==(const ThirdAgePanelTheme& p)const{return colors==p.colors;}
    static std::string hex(uint32_t color){std::string s(6,'0');for(int i=5;i>=0;--i){s[i]="0123456789ABCDEF"[color&15];color>>=4;}return s;}
    bool load(const std::string& key,const std::string& value){
        for(unsigned i=0;i<Count;++i)if(key==std::string("hd_color_")+keys[i]){
            if(value.size()!=6)return true;
            uint32_t c=0;
            for(char ch:value){int n=ch>='0'&&ch<='9'?ch-'0':ch>='a'&&ch<='f'?ch-'a'+10:ch>='A'&&ch<='F'?ch-'A'+10:-1;if(n<0)return true;c=c*16+n;}
            colors[i]=c;return true;
        }return false;
    }
};
}
