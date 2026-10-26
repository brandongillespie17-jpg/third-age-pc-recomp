#pragma once
#include <algorithm>
#include "third_age_panel_theme.h"
#include "third_age_refresh_clock.h"
#include <filesystem>
#include <fstream>
#include <string>
namespace gbarecomp {
// Captured visual preset; window, monitor, controls and gameplay stay independent.
struct ThirdAgeHdPreset {
    bool blend=true;
    bool smooth=false;
    bool expanded_view=true;
    bool hd_unit_panel=true;
    bool hd_command_panel=true;
    bool hd_battle_menus=true;
    bool ability_descriptions=true;
    bool hd_portraits=true;
    ThirdAgePanelTheme panel_theme;
    ThirdAgeHdPreset() {
        panel_theme.load("hd_color_background_top","00100D");
        panel_theme.load("hd_color_background_bottom","121E18");
        panel_theme.load("hd_color_border_light","B39B63");
        panel_theme.load("hd_color_border_dark","786943");
        panel_theme.load("hd_color_portrait_border","938254");
        panel_theme.load("hd_color_name_text","F0DEB4");
        panel_theme.load("hd_color_health_text","DEDED0");
        panel_theme.load("hd_color_stat_text","DEDED0");
        panel_theme.load("hd_color_sword_icon","F0DEB4");
        panel_theme.load("hd_color_bow_icon","F0DEB4");
        panel_theme.load("hd_color_foot_icon","F0DEB4");
        panel_theme.load("hd_color_health_fill","18DB18");
        panel_theme.load("hd_color_low_health_fill","FF0000");
        panel_theme.load("hd_color_health_track","000000");
        panel_theme.load("hd_color_spirit_text","87CBD5");
        panel_theme.load("hd_color_turn_text","F0DEB4");
        panel_theme.load("hd_color_spirit_icon","87CBD5");
        panel_theme.load("hd_color_turn_icon","F0DEB4");
        panel_theme.load("hd_color_free_move_outline","338B48");
        panel_theme.load("hd_color_rally_outline","B98AF0");
        panel_theme.load("hd_color_disorder_outline","EF6565");
        panel_theme.load("hd_color_onslaught_outline","F0A24B");
    }
    bool load(const std::string& key,const std::string& value) {
        if(panel_theme.load(key,value))return true;
        if(value!="0"&&value!="1")return false;
        if(key=="blend"){blend=value=="1";return true;}
        if(key=="smooth"){smooth=value=="1";return true;}
        if(key=="expanded_view"){expanded_view=value=="1";return true;}
        if(key=="hd_unit_panel"){hd_unit_panel=value=="1";return true;}
        if(key=="hd_command_panel"){hd_command_panel=value=="1";return true;}
        if(key=="hd_battle_menus"){hd_battle_menus=value=="1";return true;}
        if(key=="ability_descriptions"){ability_descriptions=value=="1";return true;}
        if(key=="hd_portraits"){hd_portraits=value=="1";return true;}
        return false;
    }
    void write(std::ostream& out)const {
        out<<"hd_preset_blend="<<blend<<"\n";
        out<<"hd_preset_smooth="<<smooth<<"\n";
        out<<"hd_preset_expanded_view="<<expanded_view<<"\n";
        out<<"hd_preset_hd_unit_panel="<<hd_unit_panel<<"\n";
        out<<"hd_preset_hd_command_panel="<<hd_command_panel<<"\n";
        out<<"hd_preset_hd_battle_menus="<<hd_battle_menus<<"\n";
        out<<"hd_preset_ability_descriptions="<<ability_descriptions<<"\n";
        out<<"hd_preset_hd_portraits="<<hd_portraits<<"\n";
        for(unsigned i=0;i<ThirdAgePanelTheme::Count;++i)
            out<<"hd_preset_hd_color_"<<ThirdAgePanelTheme::keys[i]<<"="<<ThirdAgePanelTheme::hex(panel_theme.colors[i])<<"\n";
    }
};
// Host preferences only. Kept separately from the game's battery save.
struct ThirdAgeSettings {
    bool mouse=true,edge_camera=true,outside_scroll=true,blend=true,smooth=false;
    bool fullscreen=false,fps=false,mute=false,native_ai=false,skip_combat=false;
    bool native_graphics=false,fast_path=false;
    bool expanded_view=true,hd_unit_panel=true,hd_command_panel=true;
    bool hd_battle_menus=true,ability_descriptions=true;
    bool hd_portraits=true; // Approved art; independent of HD panel layout.
    bool combat_skip_button=false,damage_numbers=true,fast_combat_skip=true;
    // Legacy launch flags seed new preferences; saved UI choices take priority.
    bool camera_smoothing=third_age_environment_present_hz()!=0;
    int presentation_hz=third_age_environment_present_hz()?third_age_environment_present_hz():165;
    bool sprite_cache=[](){const char* p=std::getenv("THIRD_AGE_DITHER_OAM_CACHE");return p&&std::string(p)=="1";}();
    int edge_speed=3,scale=3,volume=100;
    int resolution=0,ui_scale=100; // 0 keeps the legacy window scale; 1..4 HD presets; 5 desktop.
    bool hd_enabled=true;
    ThirdAgeHdPreset hd_preset;
    ThirdAgePanelTheme panel_theme=hd_preset.panel_theme;
    void capture_hd() {
        hd_preset.blend=blend;
        hd_preset.smooth=smooth;
        hd_preset.expanded_view=expanded_view;
        hd_preset.hd_unit_panel=hd_unit_panel;
        hd_preset.hd_command_panel=hd_command_panel;
        hd_preset.hd_battle_menus=hd_battle_menus;
        hd_preset.ability_descriptions=ability_descriptions;
        hd_preset.hd_portraits=hd_portraits;
        hd_preset.panel_theme=panel_theme;
    }
    void apply_hd() {
        blend=hd_enabled?hd_preset.blend:false;
        smooth=hd_enabled?hd_preset.smooth:false;
        expanded_view=hd_enabled?hd_preset.expanded_view:false;
        hd_unit_panel=hd_enabled?hd_preset.hd_unit_panel:false;
        hd_command_panel=hd_enabled?hd_preset.hd_command_panel:false;
        hd_battle_menus=hd_enabled?hd_preset.hd_battle_menus:false;
        ability_descriptions=hd_enabled?hd_preset.ability_descriptions:false;
        hd_portraits=hd_enabled?hd_preset.hd_portraits:false;
        panel_theme=hd_preset.panel_theme;
    }
    void set_hd_enabled(bool enabled) {
        if(enabled==hd_enabled)return;
        if(hd_enabled)capture_hd();
        hd_enabled=enabled;apply_hd();
    }
    std::filesystem::path path;
    bool save_failed=false;
    void load(const std::filesystem::path& p) {
        path=p;std::ifstream in(p);std::string line;bool command_theme_current=false,has_hd_state=false;
        while(std::getline(in,line)) {
            const auto eq=line.find('=');if(eq==std::string::npos)continue;
            const std::string k=line.substr(0,eq),v=line.substr(eq+1);
            if(k.rfind("hd_preset_",0)==0){hd_preset.load(k.substr(10),v);continue;}
            if(panel_theme.load(k,v))continue;
            int n;try{std::size_t used=0;n=std::stoi(v,&used);if(used!=v.size())continue;}catch(...){continue;}
            auto flag=[&](bool& out){if(n==0||n==1)out=n!=0;};
            if(k=="hd_enabled"){if(n==0||n==1){hd_enabled=n!=0;has_hd_state=true;}}
            else if(k=="command_theme_version"){command_theme_current=n>=1;}
            else if(k=="hd_portraits")flag(hd_portraits);else if(k=="hd_battle_menus")flag(hd_battle_menus);else if(k=="ability_descriptions")flag(ability_descriptions);else if(k=="hd_command_panel")flag(hd_command_panel);else if(k=="hd_unit_panel")flag(hd_unit_panel);else if(k=="expanded_view")flag(expanded_view);else if(k=="fast_combat_skip")flag(fast_combat_skip);else if(k=="combat_skip_button")flag(combat_skip_button);else if(k=="damage_numbers")flag(damage_numbers);
            else if(k=="native_graphics")flag(native_graphics);else if(k=="fast_path")flag(fast_path);else if(k=="skip_combat")flag(skip_combat);else if(k=="native_ai")flag(native_ai);else if(k=="mouse")flag(mouse);else if(k=="edge_camera")flag(edge_camera);
            else if(k=="outside_scroll")flag(outside_scroll);else if(k=="blend")flag(blend);
            else if(k=="smooth")flag(smooth);else if(k=="fullscreen")flag(fullscreen);
            else if(k=="fps")flag(fps);else if(k=="mute")flag(mute);
            else if(k=="camera_smoothing")flag(camera_smoothing);
            else if(k=="presentation_hz"){if(third_age_supported_present_hz(n))presentation_hz=n;}
            else if(k=="sprite_cache")flag(sprite_cache);
            else if(k=="edge_speed")edge_speed=std::clamp(n,1,6);
            else if(k=="scale")scale=std::clamp(n,1,8);
            else if(k=="resolution")resolution=std::clamp(n,0,5);
            else if(k=="ui_scale")ui_scale=std::clamp(n/25,3,6)*25;
            else if(k=="volume")volume=std::clamp(n,0,100);
        }
        // Migrate only the former default, once. Explicit custom colors survive.
        if(!command_theme_current&&panel_theme.colors[ThirdAgePanelTheme::FreeMoveOutline]==0x65d875)
            panel_theme.colors[ThirdAgePanelTheme::FreeMoveOutline]=ThirdAgePanelTheme::defaults[ThirdAgePanelTheme::FreeMoveOutline];
        // Existing files keep their individual choices. New files start with the
        // captured preset. An OFF file retains its remembered custom HD values.
        if(!has_hd_state||hd_enabled)capture_hd();
        if(has_hd_state&&!hd_enabled)apply_hd();
    }
    bool save() {
        if(hd_enabled)capture_hd();
        if(path.empty()){save_failed=true;return false;}
        auto temp=path;temp+=".tmp";
        std::ofstream out(temp,std::ios::trunc);
        out<<"# Third Age PC host settings v3\ncommand_theme_version=1\n";
        out<<"hd_enabled="<<hd_enabled<<"\n";hd_preset.write(out);
        out<<"resolution="<<resolution<<"\nui_scale="<<ui_scale<<"\n";
        for(unsigned i=0;i<ThirdAgePanelTheme::Count;++i)out<<"hd_color_"<<ThirdAgePanelTheme::keys[i]<<"="<<ThirdAgePanelTheme::hex(panel_theme.colors[i])<<"\n";
        out<<"hd_battle_menus="<<hd_battle_menus<<"\nability_descriptions="<<ability_descriptions<<"\n";
        out<<"hd_portraits="<<hd_portraits<<"\n";
        out<<"camera_smoothing="<<camera_smoothing<<"\npresentation_hz="<<presentation_hz<<"\nsprite_cache="<<sprite_cache<<"\n";
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
