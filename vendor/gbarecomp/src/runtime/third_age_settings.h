#pragma once
#include <algorithm>
#include "third_age_panel_theme.h"
#include <filesystem>
#include <fstream>
#include <string>
namespace gbarecomp {
// Host preferences only. Kept separately from the game's battery save.
struct ThirdAgeSettings {
    bool mouse=true,edge_camera=true,outside_scroll=true,blend=true,smooth=false;
    bool fullscreen=false,fps=false,mute=false,native_ai=false,skip_combat=false;
    bool native_graphics=false,fast_path=false;
    bool expanded_view=false,hd_unit_panel=false,hd_command_panel=true;
    bool hd_battle_menus=true,ability_descriptions=true;
    bool hd_portraits=true; // Approved art; independent of HD panel layout.
    bool combat_skip_button=false,damage_numbers=true,fast_combat_skip=true;
    int edge_speed=3,scale=3,volume=100;
    int resolution=0,ui_scale=100; // 0 keeps the legacy window scale; 1..4 HD presets; 5 desktop.
    ThirdAgePanelTheme panel_theme;
    std::filesystem::path path;
    bool save_failed=false;
    void load(const std::filesystem::path& p) {
        path=p;std::ifstream in(p);std::string line;bool command_theme_current=false;
        while(std::getline(in,line)) {
            const auto eq=line.find('=');if(eq==std::string::npos)continue;
            const std::string k=line.substr(0,eq),v=line.substr(eq+1);
            if(panel_theme.load(k,v))continue;
            int n;try{std::size_t used=0;n=std::stoi(v,&used);if(used!=v.size())continue;}catch(...){continue;}
            auto flag=[&](bool& out){if(n==0||n==1)out=n!=0;};
            if(k=="command_theme_version"){command_theme_current=n>=1;}
            else if(k=="hd_portraits")flag(hd_portraits);else if(k=="hd_battle_menus")flag(hd_battle_menus);else if(k=="ability_descriptions")flag(ability_descriptions);else if(k=="hd_command_panel")flag(hd_command_panel);else if(k=="hd_unit_panel")flag(hd_unit_panel);else if(k=="expanded_view")flag(expanded_view);else if(k=="fast_combat_skip")flag(fast_combat_skip);else if(k=="combat_skip_button")flag(combat_skip_button);else if(k=="damage_numbers")flag(damage_numbers);
            else if(k=="native_graphics")flag(native_graphics);else if(k=="fast_path")flag(fast_path);else if(k=="skip_combat")flag(skip_combat);else if(k=="native_ai")flag(native_ai);else if(k=="mouse")flag(mouse);else if(k=="edge_camera")flag(edge_camera);
            else if(k=="outside_scroll")flag(outside_scroll);else if(k=="blend")flag(blend);
            else if(k=="smooth")flag(smooth);else if(k=="fullscreen")flag(fullscreen);
            else if(k=="fps")flag(fps);else if(k=="mute")flag(mute);
            else if(k=="edge_speed")edge_speed=std::clamp(n,1,6);
            else if(k=="scale")scale=std::clamp(n,1,8);
            else if(k=="resolution")resolution=std::clamp(n,0,5);
            else if(k=="ui_scale")ui_scale=std::clamp(n/25,3,6)*25;
            else if(k=="volume")volume=std::clamp(n,0,100);
        }
        // Migrate only the former default, once. Explicit custom colors survive.
        if(!command_theme_current&&panel_theme.colors[ThirdAgePanelTheme::FreeMoveOutline]==0x65d875)
            panel_theme.colors[ThirdAgePanelTheme::FreeMoveOutline]=ThirdAgePanelTheme::defaults[ThirdAgePanelTheme::FreeMoveOutline];
    }
    bool save() {
        if(path.empty()){save_failed=true;return false;}
        auto temp=path;temp+=".tmp";
        std::ofstream out(temp,std::ios::trunc);
        out<<"# Third Age PC host settings v2\ncommand_theme_version=1\n";
        out<<"resolution="<<resolution<<"\nui_scale="<<ui_scale<<"\n";
        for(unsigned i=0;i<ThirdAgePanelTheme::Count;++i)out<<"hd_color_"<<ThirdAgePanelTheme::keys[i]<<"="<<ThirdAgePanelTheme::hex(panel_theme.colors[i])<<"\n";
        out<<"hd_battle_menus="<<hd_battle_menus<<"\nability_descriptions="<<ability_descriptions<<"\n";
        out<<"hd_portraits="<<hd_portraits<<"\n";
        out<<"hd_command_panel="<<hd_command_panel<<"\n";
        out<<"hd_unit_panel="<<hd_unit_panel<<"\n";
        out<<"expanded_view="<<expanded_view<<"\n";
        out<<"native_graphics="<<native_graphics<<"\nfast_path="<<fast_path<<"\n";
        out<<"skip_combat="<<skip_combat<<"\nfast_combat_skip="<<fast_combat_skip<<"\n";
        out<<"combat_skip_button="<<combat_skip_button<<"\ndamage_numbers="<<damage_numbers<<"\n";
        out<<"native_ai="<<native_ai<<"\n";
        out<<"mouse="<<mouse<<"\nedge_camera="<<edge_camera<<"\noutside_scroll="<<outside_scroll;
        out<<"\nedge_speed="<<edge_speed<<"\nblend="<<blend<<"\nsmooth="<<smooth;
        out<<"\nfullscreen="<<fullscreen<<"\nscale="<<scale<<"\nfps="<<fps;
        out<<"\nvolume="<<volume<<"\nmute="<<mute<<"\n";
        out.close();if(!out){save_failed=true;return false;}
        std::error_code ec;std::filesystem::rename(temp,path,ec);
        save_failed=bool(ec);return !save_failed;
    }
};
}
