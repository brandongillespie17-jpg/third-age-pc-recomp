#pragma once
// Exact-ROM host presentation adapter. Reads a raster-boundary snapshot;
// never changes guest RAM, OAM, VRAM, CPU registers, timing, or saves.
#include "../gba/gba_ppu.h"
#include "third_age_native_decode.h"
#include "third_age_video_capture.h"
#include "third_age_refresh_clock.h"
#include "third_age_unit_motion.h"
#include "third_age_weather.h"
#include "third_age_settings.h"
#include "third_age_unit_panel.h"
#include "third_age_command_panel.h"
#include "third_age_battle_menu.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <chrono>
#include <cstdio>
#include <memory>
#include <fstream>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
namespace gbarecomp {
class ThirdAgeView final : public gba::GbaPpu::PresentationObserver {
    ThirdAgeVideoCapture video_capture_;
    std::array<uint8_t,320*180*3> camera_world_{};
    std::array<uint8_t,320*180> camera_hud_{};
    bool camera_safe_=false,camera_backgrounds_=false;
    bool weather_candidate_=false,weather_valid_=false,weather_rain_=false;
    ThirdAgeWeatherFrame weather_frame_;
    std::array<uint8_t,320*180*3> dry_work_{},dry_on_{},weather_rgb_{};
    std::array<uint8_t,320*180> weather_mask_{},weather_foreground_{};
    // Separate from the native-space reconstruction oracle. A native viewport
    // can slide inside a boundary-clamped expanded map; screen-owned weather
    // must not inherit that moving inset when the verified host frame is shown.
    std::array<uint8_t,320*180*3> fixed_weather_rgb_{};
    std::array<uint8_t,320*180> fixed_weather_mask_{};
    std::unique_ptr<gba::GbaPpu> dry_ppu_,dry_on_ppu_,weather_front_ppu_;
    bool weather_supported(uint16_t display,const uint8_t* io)const{
        // Breach's BG2 is opaque foreground rain. Other screen-owned layers,
        // alpha effects, windows and unknown priority-zero objects remain guarded.
        if((display&0xe087u)||(display&0xe00u)!=0xe00u||!live_iw_||
           !live_iw_[0x7081]||live_iw_[0x7082]||!live_iw_[0x7083]||
           (u16(io+12)&3u)||((u16(io+12)&64u)&&(u16(io+0x4c)&255u))||!(u16(io+10)&3)||!(u16(io+14)&3))return false;
        const unsigned blend=u16(io+0x50);
        return !(((blend>>6)&3)==1&&(blend&4));
    }
    bool dry_text_supported(uint16_t display,const uint8_t* io)const{
        if((display&0xe087u)||!live_iw_)return false;
        for(unsigned bg=1;bg<4;++bg)if(display&(256u<<bg)){
            if(!live_iw_[0x7080+bg]||!(u16(io+8+2*bg)&3u))return false;
        }
        return true;
    }
    static bool has_screen_font(const uint8_t* oam){
        for(unsigned i=0;i<128;++i){const auto a0=u16(oam+8*i),a2=u16(oam+8*i+4);
            if(!(a0&0x3f00)&&!(a2&0xc00)&&(a2>>12)==1)return true;
        }
        return false;
    }
    bool fixed_weather_needed()const{return weather_candidate_&&weather_rain_&&(left_!=40||top_!=10);}
    void fixed_weather_line(int y,const uint8_t* io,const uint8_t* pal,const uint8_t* vram){
        if(!fixed_weather_needed()||y< -10||y>=170)return;
        const unsigned blend=u16(io+0x50),mode=(blend>>6)&3,fade=std::min(16u,unsigned(u16(io+0x54)&31));
        for(int x=-40;x<280;++x){
            const int wx=x+40-left_+cx_,wy=y+10-top_+cy_;
            if(wx<0||wx>=int(read(0x0300700c,2))*8-1||wy<32||wy>=int(read(0x03007004,2))*8-65)continue;
            const auto index=raster_background(2,x,y,io,vram);if(!index)continue;
            const size_t i=(y+10)*320+x+40;
            fixed_weather_mask_[i]=1;const auto color=u16(pal+2*index);
            for(unsigned ch=0;ch<3;++ch){unsigned v=(color>>(5*ch))&31;
                if(blend&4){if(mode==2)v+=(31-v)*fade/16;else if(mode==3)v-=v*fade/16;}
                fixed_weather_rgb_[3*i+ch]=(v<<3)|(v>>2);
            }
        }
    }
    void weather_line(int y,uint16_t display,const uint8_t* io,const uint8_t* pal,const uint8_t* vram,const uint8_t* oam){
        if(!weather_candidate_)return;
        if(!(weather_rain_?weather_supported(display,io):dry_text_supported(display,io))){weather_valid_=false;return;}
        // The screen overlay currently supports the native 4bpp UI font
        // bank. Unknown priority-zero sprites keep the original presentation.
        for(unsigned i=0;i<128;++i){const auto a0=u16(oam+8*i),a2=u16(oam+8*i+4);
            if((a0&0x300)!=0x200&&!(a2&0xc00)&&((a0&0x3f00)||(a2>>12)!=1))weather_valid_=false;
        }
        if(!weather_rain_)return;
        const unsigned blend=u16(io+0x50),mode=(blend>>6)&3,fade=std::min(16u,unsigned(u16(io+0x54)&31));
        for(int x=-left_;x<320-left_;++x){
            const size_t i=(y+top_)*320+x+left_;
            if(x+cx_<0||x+cx_>=int(read(0x0300700c,2))*8-1||y+cy_<32||y+cy_>=int(read(0x03007004,2))*8-65)continue;
            const auto index=raster_background(2,x,y,io,vram);if(!index)continue;
            weather_mask_[i]=1;const auto color=u16(pal+2*index);
            for(unsigned ch=0;ch<3;++ch){unsigned v=(color>>(5*ch))&31;
                if(blend&4){if(mode==2)v+=(31-v)*fade/16;else if(mode==3)v-=v*fade/16;}
                weather_rgb_[3*i+ch]=(v<<3)|(v>>2);
            }
        }
        if(y>=0&&y<160)fixed_weather_line(y,io,pal,vram);
    }
    unsigned camera_blocked_=1;uint64_t camera_key_=0;

    const gba::GbaPpu* capture_ppu_=nullptr;
    struct Unsupported {};
    struct Pixel {uint16_t palette=0,key=65535;bool semi=false,repair=false;};
    const uint8_t *live_iw_=nullptr,*live_ew_=nullptr,*rom_=nullptr;
    size_t rom_size_=0;
    ThirdAgeSettings* settings_=nullptr;
    std::array<uint8_t,32768> iw_{};
    std::array<uint8_t,262144> ew_{};
    std::array<uint8_t,98304> vr_{};
    std::array<Pixel,320*180> objects_{},objects_on_{};
    std::array<Pixel,320*180> motion_unit_{},motion_other_{};
    std::array<uint8_t,320*180> motion_sweep_{},motion_foreground_{};
    std::array<uint8_t,320*180*3> motion_under_{},motion_expected_{};
    struct MotionPosition {uint32_t id;int x,y;};
    std::unordered_map<uint32_t,MotionPosition> motion_positions_;
    ThirdAgeUnitMotionFrame motion_frame_;
    uint32_t motion_actor_=0,motion_keep_=0;unsigned motion_idle_=0;
    int motion_l_=320,motion_t_=180,motion_r_=0,motion_b_=0;
    bool motion_possible_=false;
    unsigned motion_reason_=1,motion_reject_=3;

    std::array<uint8_t,320*180*3> work_on_{};
    bool stabilize_=false;
    struct DitherObject {uint16_t a0,a1,a2;};
    std::vector<DitherObject> dither_objects_;
    // Host-only, single-frame cache. Compare the entire raster input on EVERY
    // lookup: guest CPU/DMA writes may change any OAM byte between scanlines.
    // Keep an exact semantic copy of the list, too; neither a pointer/length
    // check nor a lossy hash can safely identify its contents.
    struct DitherOamCache {
        std::array<uint8_t,1024> source{},off{},on{};
        std::array<DitherObject,128> objects{};
        size_t count=0;
        bool valid=false;
    } dither_cache_;
    static bool dither_option(const char* name){
        const char* value=std::getenv(name);return value&&std::strcmp(value,"1")==0;
    }
    const bool dither_cache_enabled_=dither_option("THIRD_AGE_DITHER_OAM_CACHE");
    // Count and time in separate diagnostic runs; clean benchmarks set neither.
    const bool dither_stats_enabled_=dither_option("THIRD_AGE_DITHER_OAM_STATS");
    const bool dither_timing_enabled_=dither_option("THIRD_AGE_DITHER_OAM_TIME");
    uint64_t dither_calls_=0,dither_hits_=0,dither_timed_calls_=0,dither_ns_=0;
    void invalidate_dither_oam_cache()noexcept{dither_cache_.valid=false;}
    bool dither_list_matches()const{
        if(dither_cache_.count!=dither_objects_.size())return false;
        for(size_t i=0;i<dither_objects_.size();++i){
            const auto& a=dither_objects_[i];const auto& b=dither_cache_.objects[i];
            if(a.a0!=b.a0||a.a1!=b.a1||a.a2!=b.a2)return false;
        }
        return true;
    }
    static bool dither_match(const uint8_t* slot,const DitherObject& d){
        // Visibility and mosaic can change in the actor before OAM is copied.
        // Keep position, shape, mode, flips, tile and palette exact; preserve
        // the raster's own mosaic bit when resolving its visibility phases.
        return (u16(slot)&~0x1200u)==(d.a0&~0x1000u)&&
            u16(slot+2)==d.a1&&u16(slot+4)==d.a2;
    }
    std::unique_ptr<gba::GbaPpu> world_on_;
    // Flank lines, spent-unit markers, and both faction stun markers.
    // 08014938 selects 080501c0 + 16*kind for the stun square, unlike
    // 08014844's spent markers. Both use the same alternating OAM phases.
    static bool dither_resource(uint32_t resource){
        return resource==0x0804ed2c||resource==0x080501f0||resource==0x08050200||
            resource==0x08050210||resource==0x08050220;
    }
    bool resolve_dither_oam(const uint8_t* source,uint8_t* off,uint8_t* on){
        if(dither_cache_enabled_){
            const bool same_source=std::memcmp(source,dither_cache_.source.data(),1024)==0;
            if(dither_cache_.valid&&same_source&&dither_list_matches()){
                std::memcpy(off,dither_cache_.off.data(),1024);
                std::memcpy(on,dither_cache_.on.data(),1024);
                return true;
            }
        }
        std::memcpy(off,source,1024);std::memcpy(on,source,1024);
        for(unsigned j=0;j<128;++j){
            for(const auto& d:dither_objects_)
                if(dither_match(source+8*j,d)){
                    off[8*j+1]|=2;on[8*j+1]&=~2u;break;
                }
        }
        if(dither_cache_enabled_){
            // Production prepare_objects is capped at 128. Fail closed if a
            // future caller exceeds that bound rather than truncating the key.
            dither_cache_.valid=dither_objects_.size()<=dither_cache_.objects.size();
            if(dither_cache_.valid){
                std::memcpy(dither_cache_.source.data(),source,1024);
                std::memcpy(dither_cache_.off.data(),off,1024);
                std::memcpy(dither_cache_.on.data(),on,1024);
                dither_cache_.count=dither_objects_.size();
                std::copy(dither_objects_.begin(),dither_objects_.end(),dither_cache_.objects.begin());
            }
        }
        return false;
    }
    void dither_oam(const uint8_t* source,uint8_t* off,uint8_t* on){
        std::chrono::steady_clock::time_point start;
        if(dither_timing_enabled_)start=std::chrono::steady_clock::now();
        const bool hit=resolve_dither_oam(source,off,on);
        if(dither_stats_enabled_){++dither_calls_;dither_hits_+=hit;}
        if(dither_timing_enabled_){
            dither_ns_+=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now()-start).count());
            ++dither_timed_calls_;
        }
    }
    std::array<uint8_t,320*180*3> work_{},latched_{};
    std::unordered_map<uint32_t,std::array<uint8_t,64>> tiles_;
    std::unordered_map<uint32_t,std::vector<uint8_t>> sprites_;
    std::unique_ptr<ThirdAgeNativeDecode> decoder_;
    // A presentation-only PPU removes BG0 before the HUD is placed on the
    // expanded canvas. Native emulation and its framebuffer remain untouched.
    std::unique_ptr<gba::GbaPpu> world_ppu_,combat_background_;
    bool combat_=false;
    unsigned mosaic_x_=1,mosaic_y_=1;
    bool black_palette_=false;
    uint64_t ready_key_=0,history_key_=0;
    std::array<uint8_t,320*180*3> fallback_{};
    struct HudPixel {uint16_t color=0,blend=0,alpha=0,fade=0;bool visible=false;};
    std::array<HudPixel,240*160> hud_{};
    bool dialog_=false;
    ThirdAgeUnitPanel hd_panel_,hd_left_panel_;
    ThirdAgeCommandPanel hd_command_;
    ThirdAgeBattleMenu hd_menu_,menu_mask_;
    unsigned menu_tail_=0;
    std::unique_ptr<gba::GbaPpu> pause_art_;
    bool pause_art_frame_=false,pause_art_ready_=false;
    ThirdAgeBattleText menu_strings_;
    ThirdAgeMenuFade menu_fade_;
    std::unordered_map<uint32_t,std::array<uint32_t,1600>> command_portraits_;
    void prepare_command(){
        hd_command_={};
        const auto scene=read(0x03004840),parent=read(0x03004860);
        // Commander resources belong to the battlefield, not the combat cut.
        // Clearing above also hides the card in combat-owned dialogs/menus.
        if(!settings_->hd_command_panel||!(scene==1||((scene==2||scene==15)&&(parent==1||parent==15))))return;
        auto r=[&](uint32_t a,unsigned n){return read(a,n);};
        auto p=third_age_command_panel(r);if(!p.valid)return;
        auto found=command_portraits_.find(p.portrait_source);
        if(found==command_portraits_.end()){
            std::array<uint32_t,1600> art{};
            if(!third_age_command_portrait(r,p.portrait_source,art))return;
            if(command_portraits_.size()>=70)command_portraits_.clear();
            found=command_portraits_.emplace(p.portrait_source,art).first;
        }
        p.portrait=found->second;p.has_portrait=true;hd_command_=std::move(p);
    }
    struct PendingHealth {uint32_t address=0;unsigned kind=0,type=0;int hp=0;};
    std::array<PendingHealth,2> pending_health_{};
    void hold_combat_health(ThirdAgeUnitPanel& p)const{
        for(const auto& h:pending_health_)if(p.valid&&h.address==p.address&&h.kind==p.kind&&h.type==p.type)
            p.hp=std::clamp(h.hp,0,p.max_hp);
    }
    uint32_t hd_map_=0;
    std::unordered_map<unsigned,std::array<uint32_t,1024>> hd_portraits_;
    void reset_hd(){hd_panel_={};hd_left_panel_={};hd_portraits_.clear();hd_map_=0;}
    uint32_t hd_combat_actor(uint32_t world,unsigned kind=5)const{
        // Combat lays out good units on the left and evil units on the right,
        // independently of who initiated the attack (see 08008cd8).
        for(unsigned off:{0x18u,0x1cu}){
            const auto a=read(world+off);
            if(a>=0x02000000&&a<=0x0203fec0&&read(a+8,1)==kind)return a;
        }
        return 0;
    }
    void prepare_hd(){
        // The host card has its own lifetime. A native menu, transition or
        // temporarily hidden BG layer must not dismiss it.
        const auto scene=read(0x03004840),parent=read(0x03004860);
        if(scene!=8&&read(0x03004880)!=8)pending_health_={};
        hd_left_panel_={};
        const bool context=scene==1||scene==8||
            ((scene==2||scene==15)&&(parent==1||parent==8||parent==15));
        if(!settings_->hd_unit_panel||!context){reset_hd();return;}
        if(scene==1){
            uint32_t key=2166136261u;
            for(unsigned i=0x6d70;i<0x6fe8;++i)key=(key^iw_[i])*16777619u;
            for(unsigned i=0x7060;i<0x70a0;++i)key=(key^iw_[i])*16777619u;
            if(hd_map_&&hd_map_!=key)reset_hd();
            hd_map_=key;
        }
        const auto last=hd_panel_;
        hd_panel_={};
        const auto world=read(0x03004898),player=read(0x03003536,1);
        if(world<0x02000000||world>0x0203fb00||player>1)return;
        if(scene==8){
            const auto a=hd_combat_actor(world,6);
            if(a){hd_left_panel_=third_age_unit_panel([&](uint32_t a,unsigned n){return read(a,n);},a);
                const auto portrait=hd_portraits_.find(hd_left_panel_.side*256+hd_left_panel_.type);
                if(portrait!=hd_portraits_.end()){hd_left_panel_.portrait=portrait->second;hd_left_panel_.has_portrait=true;}
                hold_combat_health(hd_left_panel_);
            }
        }
        const uint32_t hud=read(world+0x9c+0xdc*player);
        const uint32_t cursor=read(0x03003a90+16*player);
        const uint32_t selected=cursor>=0x02000000&&cursor<=0x0203fec0?read(cursor+96):0;
        const bool hovering=scene==1&&read(0x03004880)==1;
        uint32_t hovered=0;
        if(hovering&&cursor>=0x02000000&&cursor<=0x0203fec0){
            // HUD/selection pointers can lag behind a moving cursor. Check
            // occupied tiles, never reuse the last unit on empty terrain.
            const int tx=int32_t(read(cursor+24))/65536/32;
            const int ty=int32_t(read(cursor+28))/65536/32;
            auto matches=[&](uint32_t a){
                if(a<0x02000000||a>0x0203fec0)return false;
                const auto kind=read(a+8,1),flags=read(a+12);
                return (kind==5||kind==6)&&(flags&0x10)&&!(flags&2)&&
                    int8_t(read(a+0x87,1))>0&&
                    int32_t(read(a+24))/65536/32==tx&&int32_t(read(a+28))/65536/32==ty;
            };
            if(matches(hud))hovered=hud;
            else{
                uint32_t a=read(0x03002e40);
                for(unsigned i=0;i<512&&a>=0x02000000&&a<=0x0203fec0;++i){
                    if(matches(a)){hovered=a;break;}
                    const auto next=read(a);if(next==a)break;a=next;
                }
            }
        }
        if(hovering&&!hovered){
            hd_panel_=third_age_terrain_panel([&](uint32_t a,unsigned n){return read(a,n);},cursor);
            return;
        }
        // Follow the actor pictured by the native right combat card. Its
        // original battlefield object receives health changes during scene 8.
        const uint32_t candidates[]={hovering?hovered:(scene==8?hd_combat_actor(world):hud),hovering?0:last.address,hovering?0:selected};
        for(uint32_t address:candidates){
            if(address<0x02000000||address>0x0203fec0)continue;
            if(address==last.address&&(read(address+8,1)!=last.kind||read(address+10,2)!=last.type))continue;
            auto next=third_age_unit_panel([&](uint32_t a,unsigned n){return read(a,n);},address);
            if(!next.valid)continue;
            const auto portrait=hd_portraits_.find(next.side*256+next.type);
            if(portrait!=hd_portraits_.end()){next.portrait=portrait->second;next.has_portrait=true;}
            if(scene==8)hold_combat_health(next);
            hd_panel_=std::move(next);return;
        }
    }
    std::unique_ptr<gba::GbaPpu> native_corner_ppu_;
    std::array<uint8_t,40*40*3> native_corner_{},native_corner_work_{},native_left_{},native_left_work_{};
    bool native_left_complete_=false,native_left_ready_=false;
    bool corner_context_=false,corner_left_=false,corner_right_=false,corner_ready_=false,corner_complete_=false;
    unsigned corner_lines_=0;
    std::unique_ptr<gba::GbaPpu> command_underlay_ppu_;
    std::array<uint8_t,72*48*3> command_underlay_{},command_underlay_work_{};
    bool command_capture_=false,command_top_edge_=false,command_bottom_edge_=false,command_complete_=false,command_ready_=false;
    void capture_command_underlay(const gba::GbaPpu& ppu,unsigned y,uint16_t display,const uint8_t* io,const uint8_t* vram,const uint8_t* oam,const uint8_t* pal){
        if(y==0){
            command_complete_=command_top_edge_=command_bottom_edge_=false;
            const unsigned scene=live_iw_?live_iw_[0x4840]:0,parent=live_iw_?live_iw_[0x4860]:0;
            command_capture_=settings_&&settings_->expanded_view&&settings_->hd_command_panel&&(scene==1||(scene==2&&parent==1));
        }
        if(!command_capture_||y<112)return;
        if(display&7){command_capture_=false;return;}
        if(!command_underlay_ppu_)command_underlay_ppu_=std::make_unique<gba::GbaPpu>();
        command_underlay_ppu_->render_presentation_scanline(ppu,y,display&~256u,io,vram,oam,pal);
        if((display&256)&&!(display&128)&&(y<120||y>=152))for(unsigned x=0;x<8;++x)
            if(raster_background(0,x,y,io,vram)&&hud_visible(x,y,display,io)){
                if(y<120)command_top_edge_=true;else command_bottom_edge_=true;
            }
        if(y==159){
            command_underlay_ppu_->mark_framebuffer_latched();const auto* pixels=command_underlay_ppu_->latched_framebuffer();
            for(unsigned row=0;row<48;++row)std::memcpy(command_underlay_work_.data()+row*216,pixels+3*((row+112)*240),216);
            command_complete_=command_top_edge_&&command_bottom_edge_;
        }
    }
    void capture_native_corner(const gba::GbaPpu& ppu,uint32_t y,uint16_t display,
                               const uint8_t* io,const uint8_t* vram,const uint8_t* oam,const uint8_t* pal){
        if(y==0){
            native_left_complete_=false;corner_complete_=corner_left_=corner_right_=false;corner_lines_=0;
            const unsigned scene=live_iw_?live_iw_[0x4840]:0,parent=live_iw_?live_iw_[0x4860]:0;
            corner_context_=settings_&&settings_->expanded_view&&settings_->hd_unit_panel&&
                (scene==1||scene==8||(scene==2&&parent==1));
        }
        if(!corner_context_||y>=40)return;
        if(display&7){corner_context_=false;return;}
        if(y!=corner_lines_){corner_context_=false;return;}
        const unsigned bg=live_iw_[0x4840]==8?2:0,bit=256u<<bg;
        if(!native_corner_ppu_)native_corner_ppu_=std::make_unique<gba::GbaPpu>();
        // Use untouched raster IO, including fade and mosaic. Even when the
        // expanded terrain renderer falls back, original HUD pixels cannot leak.
        native_corner_ppu_->render_presentation_scanline(ppu,y,display&~bit,io,vram,oam,pal);
        if(y<8&&(display&bit)&&!(display&128))for(unsigned x=200;x<240;++x){
            if(raster_background(bg,x,y,io,vram)&&hud_visible(x,y,display,io,bg)){
                if(x<208)corner_left_=true;
                if(x>=232)corner_right_=true;
            }
        }
        ++corner_lines_;
        if(y==39){
            native_corner_ppu_->mark_framebuffer_latched();const auto* pixels=native_corner_ppu_->latched_framebuffer();
            for(unsigned row=0;row<40;++row)std::memcpy(native_corner_work_.data()+row*120,pixels+3*(row*240+200),120);
            corner_complete_=corner_left_&&corner_right_;
            if(live_iw_[0x4840]==8){
                for(unsigned row=0;row<40;++row)std::memcpy(native_left_work_.data()+row*120,pixels+3*row*240,120);
                native_left_complete_=true;
            }
        }
    }
    void finish_hud(){
        // Native corner cards occupy these fixed tile regions. Detect their
        // edge tiles before moving them, so a centered turn panel or dialog
        // crossing the same rows is never split into separate anchors.
        std::array<bool,30*20> occupied{};
        for(int y=0;y<160;++y)for(int x=0;x<240;++x)
            if(hud_[y*240+x].visible)occupied[(y/8)*30+x/8]=true;
        const bool bottom_card=!combat_&&occupied[19*30]&&occupied[14*30];
        const bool top_card=occupied[29]&&occupied[25];
        bool effects=black_palette_||mosaic_x_>1||mosaic_y_>1;
        for(int y=0;y<40;++y)for(int x=200;x<240;++x){
            const auto& h=hud_[y*240+x];
            if(h.visible&&(h.blend&1)&&((h.blend>>6)&3))effects=true;
        }
        const auto scene=read(0x03004840);
        const auto world=read(0x03004898),player=read(0x03003536,1);
        const bool world_valid=world>=0x02000000&&world<=0x0203fb00&&player<2;
        const uint32_t pictured=world_valid?(combat_?hd_combat_actor(world):read(world+0x9c+0xdc*player)):0;
        const bool card_matches=top_card&&(scene==1||scene==8||dialog_)&&
            hd_panel_.address&&hd_panel_.address==pictured;
        if(hd_panel_.valid&&card_matches){
            if(!effects&&!(read(world+0x168+0xdc*player)&0x604)){
                for(int y=0;y<32;++y)for(int x=0;x<32;++x){
                    const auto& h=hud_[y*240+x+208];
                    auto rgb=[](unsigned c){return (c<<3)|(c>>2);};
                    hd_panel_.portrait[y*32+x]=h.visible?0xff000000u|(rgb(h.color&31)<<16)|(rgb((h.color>>5)&31)<<8)|rgb((h.color>>10)&31):0xff15201bu;
                }
                hd_panel_.has_portrait=true;
                hd_portraits_[hd_panel_.side*256+hd_panel_.type]=hd_panel_.portrait;
            }
        }
        // The good-side combat portrait occupies native x=0..31. Keep its
        // original orientation; mirror only the HD card's layout.
        if(combat_&&hd_left_panel_.valid&&occupied[0]&&occupied[4]){
            bool left_effects=black_palette_||mosaic_x_>1||mosaic_y_>1;
            for(int y=0;y<40;++y)for(int x=0;x<40;++x){const auto& h=hud_[y*240+x];
                if(h.visible&&(h.blend&1)&&((h.blend>>6)&3))left_effects=true;
            }
            if(!left_effects){
                for(int y=0;y<32;++y)for(int x=0;x<32;++x){const auto& h=hud_[y*240+x];
                    auto rgb=[](unsigned c){return (c<<3)|(c>>2);};
                    hd_left_panel_.portrait[y*32+x]=h.visible?0xff000000u|(rgb(h.color&31)<<16)|(rgb((h.color>>5)&31)<<8)|rgb((h.color>>10)&31):0xff15201bu;
                }
                hd_left_panel_.has_portrait=true;
                hd_portraits_[hd_left_panel_.side*256+hd_left_panel_.type]=hd_left_panel_.portrait;
            }
        }
        for(int y=0;y<160;++y)for(int x=0;x<240;++x){
                const auto& h=hud_[y*240+x];if(!h.visible)continue;
                if(settings_->hd_battle_menus&&menu_tail_&&menu_mask_.valid&&
                   x>=menu_mask_.x&&x<menu_mask_.x+menu_mask_.w&&y>=menu_mask_.y&&y<menu_mask_.y+menu_mask_.h)continue;
                const bool bottom=bottom_card&&x<72&&y>=112;
                const bool top=top_card&&x>=200&&y<40;
                // Suppression belongs to composition, not to whether the HD
                // card currently has a unit. This also removes terrain cards.
                if(top&&settings_->hd_unit_panel)continue;
                if(bottom&&settings_->hd_command_panel&&hd_command_.valid)continue;
                const bool combat_left=combat_&&x<40&&y<40;
                if(combat_left&&settings_->hd_unit_panel)continue;
                const int ox=(bottom||combat_left)?0:top?80:40,oy=bottom?20:(top||combat_left)?0:10;
                auto* dst=work_.data()+3*((y+oy)*320+x+ox);
                camera_hud_[(y+oy)*320+x+ox]=1;
                const unsigned mode=(h.blend>>6)&3;
                if((h.blend&1)&&mode==1){camera_safe_=false;camera_blocked_|=64;}
                for(unsigned c=0;c<3;++c){
                    unsigned value=(h.color>>(5*c))&31;
                    if(h.blend&1){
                        if(mode==1&&(h.blend&0x3e00))value=std::min(31u,(value*std::min(h.alpha&31,16)+unsigned(dst[c]>>3)*std::min((h.alpha>>8)&31,16))/16);
                        else if(mode==2)value+=(31-value)*std::min(h.fade&31,16)/16;
                        else if(mode==3)value-=value*std::min(h.fade&31,16)/16;
                    }
                    dst[c]=uint8_t((value<<3)|(value>>2));
                }
        }
    }
    bool drawing_=false,ready_=false;
    unsigned lines_=0;
    int cx_=0,cy_=0,map_w_=0,map_h_=0;
    int left_=40,top_=10,ready_left_=40,ready_top_=10,ready_camera_x_=0,ready_camera_y_=0;
    struct TerrainCell {const std::array<uint8_t,64>* art=nullptr;uint8_t flags=0;bool depth8=false;};
    std::array<TerrainCell,3*44*26> cells_{};
    int tile_x_=0,tile_y_=0;
    uint32_t identity_=0;
    struct IdlePose {
        uint64_t tick=0;uint32_t resource=0,native=0;
        unsigned frame=0,remaining=0;
    };
    std::unordered_map<uint32_t,uint64_t> visual_ticks_;
    std::unordered_map<uint32_t,IdlePose> idle_;
    unsigned visual_frame(uint32_t a){
        const unsigned native_frame=read(a+0x117,1),timer=read(a+0x114,1);
        const uint32_t resource=read(a+0x118),flags=read(a+12);
        const unsigned signature=(native_frame<<8)|timer;
        const uint64_t tick=visual_ticks_[a];
        auto& pose=idle_[a];
        // Only the plain cyclic idle path is extended. Scripted animations,
        // native-active objects, and shared owners remain entirely game-owned.
        const unsigned kind=read(a+8,1);
        const bool flank=kind==15&&read(a+0x13c,1)==2&&resource==0x0804ed2cu;
        const bool idle=(kind==5||kind==6||flank)&&
            !(flags&0x1008)&&!(flags&0x2002000)&&
            !(read(0x030047f8)&0x4000)&&!read(a+0x11c)&&
            !(read(a+0x13d,1)&1)&&timer&&read(a+0x115,1);
        if(!idle||pose.resource!=resource||pose.native!=signature||tick<pose.tick){
            pose={tick,resource,signature,native_frame,timer};return native_frame;
        }
        const uint64_t elapsed=tick-pose.tick;pose.tick=tick;
        const unsigned count=read(read(resource+4)+6,2),duration=read(a+0x115,1);
        if(!count||count>233||pose.frame>=count||!pose.remaining||!duration)return native_frame;
        if(elapsed<pose.remaining)pose.remaining-=unsigned(elapsed);
        else {
            const uint64_t remaining=elapsed-pose.remaining;
            pose.frame=unsigned((pose.frame+1+remaining/duration)%count);
            pose.remaining=duration-unsigned(remaining%duration);
        }
        return pose.frame;
    };

    // Optional bounded, read-only capture of the live presentation inputs.
    std::ofstream effect_trace_;
    bool effect_trace_opened_=false;
    uint64_t effect_trace_frame_=0;
    void trace_effects(const uint8_t* io){
        if(!effect_trace_opened_){
            effect_trace_opened_=true;
            const char* path=std::getenv("THIRD_AGE_EFFECT_TRACE_PATH");
            if(path&&*path)effect_trace_.open(path,std::ios::out|std::ios::trunc);
        }
        if(!effect_trace_.is_open()||++effect_trace_frame_%6)return;
        if(effect_trace_.tellp()>32*1024*1024){effect_trace_.close();return;}
        const unsigned scene=iw_[0x4840];if(scene!=1&&scene!=2&&scene!=8)return;
        effect_trace_<<"{\"frame\":"<<effect_trace_frame_<<",\"scene\":"<<scene
            <<",\"camera\":["<<int32_t(read(0x03001bb0))<<','<<int32_t(read(0x03001bb4))<<','<<int32_t(read(0x03001bb8))<<','<<int32_t(read(0x03001bbc))
            <<"],\"display\":"<<u16(io)<<",\"blend\":["<<u16(io+0x50)<<','<<u16(io+0x52)<<','<<u16(io+0x54)<<"],\"actors\":[";
        bool first=true;std::unordered_set<uint32_t> seen;
        try{
            for(uint32_t a=read(0x03002e40);a;a=read(a)){
                if(!object(a)||seen.size()>=512||!seen.insert(a).second)break;
                if(!first)effect_trace_<<',';first=false;
                effect_trace_<<'['<<a;
                // address, kind, flags, x, y, sprite, frame, timer, duration,
                // script, OAM attributes, tile/palette, sharing flags, owner slot.
                for(auto field:std::array<std::pair<unsigned,unsigned>,13>{{{8,1},{12,4},{32,4},{36,4},{0x118,4},{0x117,1},{0x114,1},{0x115,1},{0x11c,4},{0x10c,4},{0x110,2},{0x13d,1},{0x13e,1}}})
                    effect_trace_<<','<<read(a+field.first,field.second);
                effect_trace_<<']';
            }
        }catch(const Unsupported&){}
        effect_trace_<<"]}\n";effect_trace_.flush();
    }

    uint32_t read(uint32_t a,unsigned n=4)const {
        const uint8_t* p=nullptr;
        if(a>=0x02000000u&&uint64_t(a)+n<=0x02040000u)p=ew_.data()+a-0x02000000u;
        else if(a>=0x03000000u&&uint64_t(a)+n<=0x03008000u)p=iw_.data()+a-0x03000000u;
        else if(a>=0x08000000u&&uint64_t(a)+n<=0x08000000ull+rom_size_)p=rom_+a-0x08000000u;
        if(!p)throw Unsupported{};
        uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(p[i])<<(8*i);return v;
    }
    static unsigned u16(const uint8_t* p){return p[0]|unsigned(p[1])<<8;}
    // The ranged split is two dedicated 4bpp tiles, not a sprite. Match the
    // authored tile bytes and the complete on-screen divider column before
    // clearing them in presentation scratch. Never paint over the final image:
    // foreground arrows, fighters, fades and layer blending must still compose.
    const uint8_t* combat_vram(const uint8_t* io,const uint8_t* vram){
        if((u16(io+10)&~64u)!=0x1e0a||u16(io+20)||u16(io+22))return vram;
        for(unsigned row=0;row<20;++row)
            if(u16(vram+0xf000+2*(row*32+14))!=0xc002||
               u16(vram+0xf000+2*(row*32+15))!=0xc003)return vram;
        for(unsigned i=0;i<32;++i)
            if(vram[0x8040+i]!=(i%4==3?0x60:0)||
               vram[0x8060+i]!=(i%4==0?0x01:0))return vram;
        // Read current raster VRAM, including any mid-frame guest changes.
        std::memcpy(vr_.data(),vram,vr_.size());
        std::memset(vr_.data()+0x8040,0,64);
        return vr_.data();
    }
    static int sign9(unsigned n){return n&256?int(n)-512:int(n);}
    static bool object(uint32_t a){return a>=0x02000000u&&a<=0x0203fec0u&&!(a&3);}
    const std::array<uint8_t,64>& tile(unsigned pool,unsigned id,bool depth8) {
        if(pool>1||id>65535)throw Unsupported{};
        const uint32_t key=id|(pool<<16)|(unsigned(depth8)<<17);
        auto found=tiles_.find(key);if(found!=tiles_.end())return found->second;
        std::array<uint8_t,64> out{};
        if(depth8){
            const auto src=read(0x03001ba0+4*pool)+4+64*id;
            for(unsigned i=0;i<64;++i)out[i]=read(src+i,1);
        }else{
            const auto table=read(0x03006d7c+16*pool);
            uint32_t bits=read(read(0x03006d78+16*pool)+4*id);
            for(unsigned i=0;i<32;++i){
                unsigned value=0,depth=0;
                for(;depth<18;++depth){
                    if(bits/8>=rom_size_)throw Unsupported{};
                    value=value*2+((rom_[bits/8]>>(bits%8))&1);++bits;
                    const unsigned count=read(table+depth,1);
                    if(value<count){
                        const unsigned index=read(table+18+depth,1)+value;
                        if(index>255)throw Unsupported{};
                        out[i]=read(table+36+index,1);break;
                    }
                    value-=count;
                }
                if(depth==18)throw Unsupported{};
            }
        }
        if(tiles_.size()>=16384)throw Unsupported{};
        return tiles_.emplace(key,out).first->second;
    }
    const std::vector<uint8_t>& sprite(uint32_t src) {
        auto found=sprites_.find(src);if(found!=sprites_.end())return found->second;
        if(src<0x08000000u)throw Unsupported{};
        const auto header=read(src),size=header>>8;
        if(size==0||size>32768)throw Unsupported{};
        if((header&255)==0){
            // Onslaught mixes compressed and raw frames in one animation.
            // A zero type is a size-prefixed byte copy, just as in the native
            // loader. Bound every source read and keep all output host-owned.
            std::vector<uint8_t> out(size);
            for(unsigned i=0;i<size;++i)out[i]=read(src+4+i,1);
            if(sprites_.size()>=1024)sprites_.clear();
            return sprites_.emplace(src,std::move(out)).first->second;
        }
        if((header&255)==0x10){
            // Flag frames 14..17 use BIOS LZ77, unlike their 0x60 siblings.
            // Decode only into host storage; read() bounds every ROM access.
            std::vector<uint8_t> out;out.reserve(size);uint32_t input=src+4;
            while(out.size()<size){
                const unsigned flags=read(input++,1);
                for(unsigned mask=128;mask&&out.size()<size;mask>>=1){
                    if(!(flags&mask)){out.push_back(read(input++,1));continue;}
                    const unsigned hi=read(input++,1),lo=read(input++,1);
                    const unsigned count=(hi>>4)+3,distance=((hi&15)<<8)+lo+1;
                    if(distance>out.size()||count>size-out.size())throw Unsupported{};
                    for(unsigned j=0;j<count;++j)out.push_back(out[out.size()-distance]);
                }
            }
            if(sprites_.size()>=1024)sprites_.clear();
            return sprites_.emplace(src,std::move(out)).first->second;
        }
        if((header&255)!=0x60)throw Unsupported{};
        if(!decoder_)decoder_=std::make_unique<ThirdAgeNativeDecode>();
        uint32_t regs[16]{};regs[0]=src+4;regs[1]=0x06010000;regs[13]=0x03007e00;
        regs[14]=0x08000000;regs[15]=0x03003b00;
        if(!decoder_->evaluate(regs,31,ew_.data(),iw_.data(),vr_.data(),rom_,rom_size_))throw Unsupported{};
        // Accept only scratch stack and the expected output region.
        for(const auto& w:decoder_->writes)
            if(!((w.address>=0x06010000&&uint64_t(w.address)+w.width<=0x06010000ull+size)||
                 (w.address>=0x03007d80&&uint64_t(w.address)+w.width<=0x03007e00)))throw Unsupported{};
        if(sprites_.size()>=1024)sprites_.clear();
        return sprites_.emplace(src,std::vector<uint8_t>(decoder_->vr.begin()+65536,decoder_->vr.begin()+65536+size)).first->second;
    }
    bool raster_part_match(const uint8_t* slot,const DitherObject& expected,
                           uint32_t resource,unsigned pose,unsigned part_index,uint32_t part,unsigned bytes){
        // Sprite allocation in RAM can already name the next upload while
        // this raster still uses the previous allocation. Identify that old
        // part by its actual pixels, never by a loose tile-number range.
        if((u16(slot)&~0x1200u)!=(expected.a0&~0x1000u)||
           u16(slot+2)!=expected.a1||(u16(slot+4)&0xfc00)!=(expected.a2&0xfc00))return false;
        const unsigned at=65536+(u16(slot+4)&1023)*32;
        if(at+bytes>vr_.size())return false;
        const auto anim=read(resource+4);const unsigned count=read(anim+6,2);
        if(!count||count>233||pose>=count)return false;
        // Dither cycles are tiny. For longer attached effects, bound work to
        // the current pose and its immediate neighbours at an upload handoff.
        const unsigned candidates[]={pose,(pose+count-1)%count,(pose+1)%count};
        for(unsigned candidate=0;candidate<(count<=4?count:3);++candidate){
            const unsigned f=count<=4?candidate:candidates[candidate];
            const auto info=anim+12+read(anim+12+2*f,2);
            const auto parts=info+10+6*read(anim+11,1)+2*read(anim+10,1);
            if(part_index>=(read(info,1)&31)||read(parts+4*part_index)!=part)continue;
            try{
                const auto& art=sprite(read(resource)+read(info+4,2));
                const unsigned offset=(part>>22)*32;
                if(offset+bytes<=art.size()&&std::equal(art.begin()+offset,art.begin()+offset+bytes,vr_.begin()+at))return true;
            }catch(const Unsupported&){/* An unrecognised neighbour is not a match. */}
        }
        return false;
    }
    void prepare_range(){
        // The game's kind-25 overlay emits only native-visible OAM quads.
        // Its reachability grid already covers the whole movement range.
        const uint32_t a=read(0x03004914),grid=read(0x03004818);
        if(!object(a)||read(a+8,1)!=25||read(a+0x118)!=0x08053f4c||
           !(read(a+12)&0x80000u)||(read(a+12)&0x200000u))return;
        const unsigned n=read(grid+24,2),radius=read(0x03004920,2);
        const int ox=read(0x03004910,1),oy=read(0x03004911,1);
        if(!n||n>256||radius>255||ox>=int(n)||oy>=int(n))return;
        const auto& art=sprite(read(0x08053f4c));
        if(art.size()!=1024)throw Unsupported{};
        const unsigned bank=read(a+0x110,2)>>12;
        const int width=int(read(0x0300700c,2))*8;
        const int height=int(read(0x03007004,2))*8;
        for(int gx=std::max(0,ox-int(radius));gx<=std::min(int(n)-1,ox+int(radius));++gx){
            const auto column=read(read(grid)+4*gx);
            for(int gy=std::max(0,oy-int(radius));gy<=std::min(int(n)-1,oy+int(radius));++gy){
                if(int16_t(read(column+22*gy+16,2))>394)continue;
                const int wx=gx*32,wy=(gy+1)*32,sx=wx-cx_,sy=wy-cy_;
                if(sx+32<=-left_||sx>=320-left_||sy+32<=-top_||sy>=180-top_)continue;
                if(wx<0||wy<0||wx>=width||wy>=height)continue;
                const auto meta=read(read(0x03001978)+2*((wy/32)*(width/32)+wx/32),2);
                unsigned priority=((read(read(0x03001974)+16*meta,1)>>5)+1)&3;
                if(!priority)priority=1;
                for(int y=std::max(sy,-top_);y<std::min(sy+32,180-top_);++y)
                    for(int x=std::max(sx,-left_);x<std::min(sx+32,320-left_);++x){
                        const unsigned px=x-sx,py=y-sy;
                        const unsigned off=((py/8)*4+px/8)*32+(py%8)*4+(px%8)/2;
                        const unsigned color=(art[off]>>(4*(px%2)))&15;if(!color)continue;
                        const Pixel value{uint16_t(256+bank*16+color),uint16_t(priority*1024+511),true,false};
                        objects_[(y+top_)*320+x+left_]=value;
                        objects_on_[(y+top_)*320+x+left_]=value;
                    }
            }
        }
    }
    void prepare_objects(const uint8_t* oam){
        invalidate_dither_oam_cache(); // Before clear/rebuild, including throwing paths.
        objects_.fill(Pixel{});objects_on_.fill(Pixel{});dither_objects_.clear();bool has_dither=false;prepare_range();
        std::vector<uint32_t> actors;std::unordered_set<uint32_t> seen;
        for(uint32_t a=read(0x03002e40);a;a=read(a)){
            if(!object(a)||seen.size()>=512||!seen.insert(a).second)throw Unsupported{};
            const auto flags=read(a+12);
            // Native OAM alternates its visibility buffer for dithered flank
            // borders and spent-unit markers. Match that phase so the host
            // frame blend remains continuous across the native viewport.
            const bool stable=stabilize_&&(read(a+0x13d,1)&0x20)&&dither_resource(read(a+0x118));
            if(!stable&&(read(a+0x13d,1)&0x20)&&(read(0x03001948)&1))continue;
            const bool cursor=a==read(0x03003a90+16*read(0x03003536,1));
            // Movement confirmation disables the cursor with bit 0 while its
            // sprite data and bit 0x10 remain set. Extend an active cursor past
            // native culling, but never resurrect an explicitly hidden one.
            if(cursor&&!(flags&1u))continue;
            const bool flank=read(a+8,1)==15&&read(a+0x13c,1)==2&&read(a+0x118)==0x0804ed2c;
            if((!(flags&16)&&!stable)||(flags&0x200000u)||!read(a+0x118)||(!(flags&0x80000000u)&&!cursor&&!flank&&!stable))continue;
            const int x=int32_t(read(a+32))/65536-cx_,y=int32_t(read(a+36))/65536-cy_;
            if(x< -168||x>408||(!flank&&(y< -138||y>298)))continue;
            if(weather_candidate_&&!(read(a+0x110,2)&0xc00u))weather_valid_=false;
            actors.push_back(a);
        }
        motion_actor_=0;motion_possible_=false;motion_reject_=3;motion_l_=320;motion_t_=180;motion_r_=motion_b_=0;
        if(third_age_present_hz()&&third_age_unit_motion_enabled()&&!dialog_&&!combat_){
            std::unordered_map<uint32_t,MotionPosition> next;
            unsigned moving=0;
            for(auto a:actors)if(read(a+8,1)==5||read(a+8,1)==6){
                const MotionPosition pos{read(a+10,2)|(read(a+8,1)<<16),int32_t(read(a+32))/65536,int32_t(read(a+36))/65536};
                next[a]=pos;auto it=motion_positions_.find(a);
                if(it!=motion_positions_.end()&&it->second.id==pos.id&&
                   (it->second.x!=pos.x||it->second.y!=pos.y)){
                    ++moving;if(std::abs(it->second.x-pos.x)<=8&&std::abs(it->second.y-pos.y)<=8)motion_actor_=a;
                }
            }
            if(moving>1){motion_actor_=motion_keep_=0;}
            else if(motion_actor_){motion_keep_=motion_actor_;motion_idle_=0;}
            else if(!moving&&motion_keep_&&next.count(motion_keep_)&&++motion_idle_<=2)motion_actor_=motion_keep_;
            else motion_keep_=0;
            motion_positions_=std::move(next);
            if(motion_actor_){motion_possible_=true;motion_unit_.fill(Pixel{});motion_other_.fill(Pixel{});}
        }else{motion_positions_.clear();motion_keep_=0;}
        // Match battlefield depth: lower feet first, then the rightmost unit
        // on equal rows. The x tie-break matters when a culled unit overlaps
        // a large native sprite at a viewport boundary.
        std::stable_sort(actors.begin(),actors.end(),[&](auto a,auto b){
            const auto ay=int32_t(read(a+36)),by=int32_t(read(b+36));
            return ay!=by?ay>by:int32_t(read(a+32))>int32_t(read(b+32));
        });
        static constexpr int widths[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}};
        static constexpr int heights[3][4]={{8,16,32,64},{8,8,16,32},{16,32,32,64}};
        // 08001d48 draws attachments from each unit's +140/+144 slots.
        // One kind-31 ability actor is reused at every affected unit; its own
        // position/priority only retain the LAST native-visible target.
        struct Instance {uint32_t sprite,anchor;bool attached,mirror;};
        std::vector<Instance> instances;
        for(auto parent:actors){
            auto attached=[&](unsigned phase){
                const unsigned kind=read(parent+8,1);
                if((kind!=5&&kind!=6)||(read(parent+12)&2))return;
                for(unsigned slot=0;slot<2;++slot){
                    if(slot==1&&read(parent+0x4d,1)==1)continue;
                    const uint32_t effect=read(parent+0x140+4*slot);
                    if(!effect)continue;
                    if(!object(effect))throw Unsupported{};
                    if(read(effect+8,1)!=31)continue;
                    const auto flags=read(effect+12),sharing=read(effect+0x13d,1);
                    if(!(sharing&phase)||!(flags&16)||(flags&(2u|0x200000u))||!read(effect+0x118))continue;
                    if((sharing&0x20)&&(read(0x03001948)&1))continue;
                    // The optional reflected pass is used by mirrored effects.
                    if(sharing&0x10)instances.push_back({effect,parent,true,true});
                    instances.push_back({effect,parent,true,false});
                }
            };
            attached(4);instances.push_back({parent,parent,false,false});attached(8);
        }
        unsigned order=0;
        for(const auto& instance:instances){
            const auto a=instance.sprite,anchor=instance.anchor;
            uint32_t owner=a;
            if(read(a+0x13d,1)&1){const auto p=read(read(0x0300319c)+44*read(a+0x13e,1));if(p){if(!object(p))throw Unsupported{};owner=p;}}
            const auto resource=read(owner+0x118),anim=read(resource+4);
            const bool stable=stabilize_&&(read(a+0x13d,1)&0x20)&&dither_resource(resource);
            if(motion_actor_&&anchor==motion_actor_&&(instance.attached||(read(a+0x10c)&0x2f00u)||(weather_candidate_&&!(read(a+0x110,2)&0xc00u)))){motion_possible_=false;motion_reject_=31;}

            has_dither|=stable;
            const unsigned pose=visual_frame(owner);
            const auto info=anim+12+read(anim+12+2*pose,2);
            const auto parts=info+10+6*read(anim+11,1)+2*read(anim+10,1);
            auto attrs=read(a+0x10c),attr2=read(a+0x110,2);
            if(instance.attached){
                attr2=(attr2&~0x0c00u)|(read(anchor+0x110,2)&0x0c00u);
                if(read(a+0x13d,1)&0x10)
                    attrs=instance.mirror?(attrs|0x10000000u):(attrs&~0x10000000u);
            }
            const bool flank=read(a+8,1)==15&&read(a+0x13c,1)==2&&resource==0x0804ed2c;
            const int origin_x=int32_t(read(anchor+32))/65536-cx_,origin_y=flank?-(cy_&31):int32_t(read(anchor+36))/65536-cy_;
            // Culled units and ground markers can cross back into the native
            // rectangle. Recover their full silhouettes across that boundary.
            const bool unit=read(a+8,1)==5||read(a+8,1)==6;
            const bool repair=(unit||dither_resource(resource))&&!(read(a+12)&0x40000000u)&&
                (origin_x<0||origin_x>=240||origin_y<0||origin_y>=160);
            const std::vector<uint8_t>* art=nullptr;
            for(unsigned j=0;j<(read(info,1)&31);++j){
                const auto part=read(parts+4*j);const unsigned shape=(part>>20)&3,size=(part>>18)&3;
                if(shape==3)throw Unsupported{};
                const int w=widths[shape][size],h=heights[shape][size];
                int dx=sign9(part&511),dy=sign9((part>>9)&511);
                const bool flipx=attrs&0x10000000u,flipy=attrs&0x20000000u;
                if(flipx)dx=-dx-w;if(flipy)dy=-dy-h;
                for(int repeat=flank?-1:0;repeat< (flank?7:1);++repeat){
                const int sx=origin_x+dx,sy=origin_y+dy+32*repeat;
                // Match complete native OAM attributes, including position,
                // shape, palette and tile. Disabled stale slots and unrelated
                // sprites must never be enabled merely for sharing a tile.
                bool native_part=false;
                if((stable||instance.attached)&&!(attrs&0x2300u)){
                    const unsigned tile_id=part>>22;
                    const DitherObject part_attrs{uint16_t((attrs&0x3c00)|(shape<<14)|(unsigned(sy)&255)),
                        uint16_t(((attrs>>16)&0x3e00)|(size<<14)|(unsigned(sx)&511)),
                        uint16_t((attr2&0xfc00)|(((attr2&1023)+tile_id)&1023))};
                    if(stable){
                        if(dither_objects_.size()>=128)throw Unsupported{};
                        dither_objects_.push_back(part_attrs);
                    }
                    for(unsigned slot=0;slot<128;++slot){
                        if(!stable&&(u16(oam+8*slot)&0x0300)==0x0200)continue;
                        if(dither_match(oam+8*slot,part_attrs)){native_part=true;break;}
                    }
                    // A current-allocation match wins. Older disabled slots
                    // are leftover OAM, not evidence of a visible old upload.
                    for(unsigned slot=0;!native_part&&slot<128;++slot){
                        if((u16(oam+8*slot)&0x0300)==0x0200)continue;
                        if(raster_part_match(oam+8*slot,part_attrs,resource,pose,j,part,unsigned(w*h/2))){
                            if(stable){
                                if(dither_objects_.size()>=128)throw Unsupported{};
                                dither_objects_.push_back({uint16_t(u16(oam+8*slot)&~0x0200u),
                                    uint16_t(u16(oam+8*slot+2)),uint16_t(u16(oam+8*slot+4))});
                            }
                            native_part=true;
                        }
                    }
                }
                // Culling flags describe the next object update, not always
                // this raster. Stabilize an existing native part regardless
                // of those flags, and recover a missing part across the native
                // rectangle even when its anchor is still just inside it.
                const bool part_repair=(stable||instance.attached)?!native_part:repair;
                if(sx+w<=-left_||sx>=320-left_||sy+h<=-top_||sy>=180-top_)continue;
                // Affine/8bpp world metasprites need a separate authored path.
                if(attrs&0x2300u)throw Unsupported{};
                if(!art)art=&sprite(read(resource)+read(info+4,2));
                const unsigned tile_id=part>>22;
                if((tile_id+unsigned(w*h/64))*32>art->size())throw Unsupported{};
                const uint16_t key=uint16_t(((attr2>>10)&3)*1024+std::min(order,1023u));
                for(int y=std::max(sy,-top_);y<std::min(sy+h,180-top_);++y){
                    for(int x=std::max(sx,-left_);x<std::min(sx+w,320-left_);++x){
                        const int px=flipx?w-1-(x-sx):x-sx,py=flipy?h-1-(y-sy):y-sy;
                        const unsigned off=(tile_id+(py/8)*(w/8)+px/8)*32+(py%8)*4+(px%8)/2;
                        const unsigned color=((*art)[off]>>(4*(px%2)))&15;if(!color)continue;
                        auto& pixel=objects_[(y+top_)*320+x+left_];
                        const Pixel value{uint16_t(256+(attr2>>12)*16+color),key,((attrs>>10)&3)==1,part_repair};
                        if(motion_actor_){
                            const int mx=x+left_,my=y+top_;auto& out=(anchor==motion_actor_?motion_unit_:motion_other_)[my*320+mx];
                            if(key<out.key)out=value;
                            if(anchor==motion_actor_){motion_l_=std::min(motion_l_,mx);motion_r_=std::max(motion_r_,mx+1);motion_t_=std::min(motion_t_,my);motion_b_=std::max(motion_b_,my+1);}
                        }
                        if(!stable&&key<pixel.key)pixel=value;
                        if(stabilize_){auto& on=objects_on_[(y+top_)*320+x+left_];if(key<on.key)on=value;}
                    }
                }
                ++order;
                }
            }
        }
        stabilize_=stabilize_&&has_dither;
        // Keep cutouts well inside the canvas so camera backing textures cannot
        // expose a baked-in copy. Reject overlap and clipping over the whole
        // interpolation corridor, including transparent sprite pixels.
        if(motion_possible_){
            if(motion_l_<24||motion_t_<24||motion_r_>296||motion_b_>156||motion_r_<=motion_l_||motion_b_<=motion_t_){motion_possible_=false;motion_reject_=32;}
            if(motion_possible_){
                // Dilate occupied row spans by the maximum presentation offset.
                // Empty corners of the bounding box are not part of the path.
                motion_sweep_.fill(0);motion_foreground_.fill(0);
                for(int sy=motion_t_;sy<motion_b_;++sy){
                    int lo=motion_r_,hi=motion_l_;
                    for(int x=motion_l_;x<motion_r_;++x)if(motion_unit_[sy*320+x].palette){lo=std::min(lo,x);hi=std::max(hi,x+1);}
                    if(lo>=hi)continue;
                    for(int y=sy-8;y<=sy+8;++y)std::fill(motion_sweep_.begin()+y*320+lo-8,motion_sweep_.begin()+y*320+hi+8,1);
                }
                for(int y=motion_t_-8;y<motion_b_+8;++y)for(int x=motion_l_-8;x<motion_r_+8;++x)
                    if(motion_sweep_[y*320+x]&&motion_other_[y*320+x].palette){motion_possible_=false;motion_reject_=33;}
            }
        }

    }
    void prepare_terrain(const uint8_t* io){
        tile_x_=(cx_-left_)/8-1;tile_y_=(cy_-top_)/8-1;
        cells_.fill(TerrainCell{});
        for(unsigned bg=1;bg<4;++bg){
            if(!read(0x03007080+bg,1))continue;
            const auto map=read(0x03007060+4*bg),defs=read(0x03007070+4*bg);
            const auto flags_base=defs+read(0x03007090+4*bg);
            const unsigned pool=read(0x03007010+2*bg,2);const bool depth8=u16(io+8+2*bg)&128;
            for(int ty=0;ty<26;++ty)for(int tx=0;tx<44;++tx){
                const int wx=tile_x_+tx,wy=tile_y_+ty;
                if(wx<0||wy<0||wx>=map_w_*4||wy>=map_h_*4)continue;
                const unsigned meta=read(map+2*((wy/4)*map_w_+wx/4),2),index=(wy%4)*4+wx%4;
                const unsigned id=read(defs+meta*32+index*2,2),flags=read(flags_base+meta*16+index,1);
                cells_[(bg-1)*44*26+ty*44+tx]={&tile(pool,id,depth8),uint8_t(flags),depth8};
            }
        }
    }
    // Weather and other non-map text backgrounds own a native VRAM tilemap.
    // Their scroll is independent of the terrain camera (Moria's snow is BG2).
    unsigned raster_background(unsigned bg,int x,int y,const uint8_t* io,const uint8_t* vram)const{
        const unsigned cnt=u16(io+8+2*bg),size=cnt>>14;
        const unsigned w=(size&1)?512:256,h=(size&2)?512:256;
        const unsigned sx=unsigned(x+int(u16(io+16+4*bg)))&(w-1),sy=unsigned(y+int(u16(io+18+4*bg)))&(h-1);
        const unsigned block=(sx/256)+(sy/256)*(w/256);
        const unsigned map=((cnt>>8)&31)*2048+block*2048+2*((sy/8%32)*32+sx/8%32);
        if(map+1>=98304)throw Unsupported{};
        const unsigned entry=u16(vram+map);unsigned px=sx%8,py=sy%8;
        if(entry&1024)px=7-px;if(entry&2048)py=7-py;
        const unsigned base=((cnt>>2)&3)*16384,id=entry&1023;
        const bool depth8=cnt&128;
        const unsigned at=base+id*(depth8?64:32)+py*(depth8?8:4)+(depth8?px:px/2);
        if(at>=98304)throw Unsupported{};
        const unsigned color=depth8?vram[at]:(vram[at]>>(4*(px%2)))&15;
        return color?(depth8?color:(entry>>12)*16+color):0;
    }
    static bool hud_visible(int x,int y,uint16_t display,const uint8_t* io,unsigned bg=0){
        if(!(display&0x6000u))return true;
        auto inside=[](int p,int start,int end){return start<=end?(p>=start&&p<end):(p>=start||p<end);};
        for(unsigned w=0;w<2;++w){
            if(!(display&(0x2000u<<w)))continue;
            const unsigned h=u16(io+0x40+2*w),v=u16(io+0x44+2*w);
            if(inside(x,h>>8,h&255)&&inside(y,v>>8,v&255))return io[0x48+w]&(1u<<bg);
        }
        return io[0x4a]&(1u<<bg);
    }
    unsigned terrain(unsigned bg,int x,int y,const uint8_t* io,const uint8_t* vram){
        if(!read(0x03007080+bg,1)){
            return raster_background(bg,x,y,io,vram);
        }

        const unsigned hofs=u16(io+16+4*bg),vofs=u16(io+18+4*bg);
        const int wx=x+cx_+((int(hofs)-cx_+128)&255)-128;
        const int wy=y+cy_+((int(vofs)-cy_+128)&255)-128;
        if(wx<0||wy<0||wx>=map_w_*32||wy>=map_h_*32)return 0;
        const int tx=wx/8-tile_x_,ty=wy/8-tile_y_;
        if(tx<0||tx>=44||ty<0||ty>=26)throw Unsupported{};
        const auto& cell=cells_[(bg-1)*44*26+ty*44+tx];if(!cell.art)return 0;
        int px=wx%8,py=wy%8;if(cell.flags&1)px=7-px;if(cell.flags&2)py=7-py;
        const auto& art=*cell.art;
        const unsigned c=cell.depth8?art[py*8+px]:(art[py*4+px/2]>>(4*(px%2)))&15;
        return c?(cell.depth8?c:((cell.flags>>2)&15)*16+c):0;
    }
    void line(int y,uint16_t display,const uint8_t* io,const uint8_t* pal,const uint8_t* vram,bool on=false,bool motion=false,bool dry=false){
        if(weather_rain_&&(dry||(motion&&weather_candidate_)))display&=~1024u;
        auto& canvas=dry?(on?dry_on_:dry_work_):motion?motion_under_:(on?work_on_:work_);
        const auto& objects=on?objects_on_:objects_;
        // Battlefield dialogs use windows to clip BG0. Accept them only when
        // every region leaves all world layers and effects enabled.
        if(display&0x8000u)throw Unsupported{};
        if(display&0x6000u){
            if((io[0x4a]&0x3e)!=0x3e||
               ((display&0x2000u)&&(io[0x48]&0x3e)!=0x3e)||
               ((display&0x4000u)&&(io[0x49]&0x3e)!=0x3e))throw Unsupported{};
        }
        const unsigned mosaic=u16(io+0x4c);
        for(unsigned bg=1;bg<4;++bg)
            if((display&(256u<<bg))&&(u16(io+8+2*bg)&64)&&(mosaic&255))throw Unsupported{};
        if((display&4096)&&(mosaic&0xff00))throw Unsupported{};
        struct Candidate{unsigned color,key,layer;bool semi;};
        const unsigned blend=u16(io+0x50),mode=(blend>>6)&3;
        const unsigned eva=std::min(u16(io+0x52)&31,16u),evb=std::min((u16(io+0x52)>>8)&31,16u),evy=std::min(u16(io+0x54)&31,16u);
        for(int x=-left_;x<320-left_;++x){
            if(motion&&(x+left_<motion_l_-8||x+left_>=motion_r_+8||y+top_<motion_t_-8||y+top_>=motion_b_+8))continue;
            if(!motion&&x>=0&&x<240&&y>=0&&y<160&&
               !objects[(y+top_)*320+x+left_].repair)continue;
            Candidate top{0x0821,65534,5,false},second{0,65535,5,false};
            auto submit=[&](Candidate c){if(c.key<top.key){second=top;top=c;}else if(c.key<second.key)second=c;};
            if(!(display&128)){
                for(unsigned bg=1;bg<4;++bg)if(display&(256u<<bg)){
                    const unsigned c=terrain(bg,x,y,io,vram);if(c)submit({u16(pal+2*c),(u16(io+8+2*bg)&3)*1024+512+bg,bg,false});
                }
                const auto& obj=objects[(y+top_)*320+x+left_];
                if(!motion&&(display&4096)&&obj.palette&&!(dry&&obj.key<512))submit({u16(pal+2*obj.palette),obj.key,4,obj.semi});
            }
            if(motion){
                const auto& unit=motion_unit_[(y+top_)*320+x+left_];
                if((mode==1&&(blend&16))||(display&0x6000u)||(display&128)){motion_possible_=false;motion_reject_=34;}
                // Retain foreground terrain over the swept silhouette so branches
                // still occlude the unit at intermediate positions.
                const auto priority=read(motion_actor_+0x110,2)>>10&3;
                if(motion_sweep_[(y+top_)*320+x+left_]&&top.key<priority*1024)motion_foreground_[(y+top_)*320+x+left_]=1;
                if(unit.palette){
                    const auto c=u16(pal+2*unit.palette);auto* dst=motion_expected_.data()+3*((y+top_)*320+x+left_);
                    for(unsigned ch=0;ch<3;++ch){unsigned v=(c>>(5*ch))&31;if(blend&16){if(mode==2)v+=(31-v)*evy/16;else if(mode==3)v-=v*evy/16;}dst[ch]=(v<<3)|(v>>2);}
                }
            }
            unsigned col=top.color;
            const bool outside=!combat_&&(x+cx_<0||x+cx_>=int(read(0x0300700c,2))*8-1||y+cy_<32||y+cy_>=int(read(0x03007004,2))*8-65);
            if(outside){auto* out=canvas.data()+3*((y+top_)*320+x+left_);out[0]=7;out[1]=11;out[2]=20;continue;}
            if(display&128)col=0x7fff;
            else{
                const bool first=blend&(1u<<top.layer),target2=blend&(256u<<second.layer);
                const bool alpha=target2&&(top.semi||(mode==1&&first));
                if(alpha||(first&&(mode==2||mode==3))){
                    col=0;
                    for(unsigned shift:{0u,5u,10u}){
                        unsigned c=(top.color>>shift)&31;
                        if(alpha)c=std::min(31u,(c*eva+((second.color>>shift)&31)*evb)/16);
                        else if(mode==2)c+=(31-c)*evy/16;else c-=c*evy/16;
                        col|=c<<shift;
                    }
                }
            }
            auto* out=canvas.data()+3*((y+top_)*320+x+left_);
            for(unsigned channel=0;channel<3;++channel){unsigned c=(col>>(5*channel))&31;out[channel]=(c<<3)|(c>>2);}
        }
    }
public:
    // Observe original object-update calls without changing guest state or time.
    // This gives frozen margin units and flank borders their native cadence.
    void object_tick(uint32_t a){
        if(!settings_||!settings_->expanded_view||!live_iw_||live_iw_[0x4840]!=1||!object(a))return;
        const auto* p=live_ew_+a-0x02000000u;
        const unsigned kind=p[8];
        const bool flank=kind==15&&p[0x13c]==2&&
            (uint32_t(p[0x118])|(uint32_t(p[0x119])<<8)|(uint32_t(p[0x11a])<<16)|(uint32_t(p[0x11b])<<24))==0x0804ed2cu;
        if(kind!=5&&kind!=6&&!flank)return;
        if(visual_ticks_.size()>=1024){visual_ticks_.clear();idle_.clear();}
        ++visual_ticks_[a];
    }
    uint64_t history_key()const{return history_key_;}
    const ThirdAgeCommandPanel& command_panel()const{return hd_command_;}
    ThirdAgeBattleMenu battle_menu()const{auto p=hd_menu_;p.offset_x=ready_?40:ready_left_;p.offset_y=ready_?10:ready_top_;return p;}
    const ThirdAgeUnitPanel& unit_panel()const{return hd_panel_;}
    const ThirdAgeUnitPanel& left_unit_panel()const{return hd_left_panel_;}
    // Exact-ROM combat callbacks: retain pre-calculation HP until the native
    // result/health-display routine runs. Presentation only, no guest writes.
    void combat_health_event(uint32_t pc,uint32_t a){
        if(!live_iw_||!live_ew_||live_iw_[0x4840]!=8||a<0x02000000||a>0x0203fec0)return;
        const auto* p=live_ew_+a-0x02000000;const unsigned kind=p[8],type=u16(p+10);
        if(kind!=5&&kind!=6)return;
        auto& h=pending_health_[kind-5];
        if(pc==0x08008238u)h={a,kind,type,std::max(0,int(int8_t(p[0x87])))};
        else if(pc==0x08008360u&&h.address==a)h={};
    }
    const ThirdAgeUnitMotionFrame* unit_motion()const{return ready_&&motion_frame_.valid()?&motion_frame_:nullptr;}
    unsigned unit_motion_reason()const{return motion_reason_;}
    const ThirdAgeWeatherFrame* weather()const{return ready_&&camera_safe_&&weather_frame_.valid()?&weather_frame_:nullptr;}
    const uint8_t* camera_world()const{return ready_&&camera_safe_?camera_world_.data():nullptr;}
    const uint8_t* camera_hud()const{return camera_hud_.data();}
    uint64_t camera_scene_key()const{return camera_key_;}
    unsigned camera_blocked()const{return !ready_?1:camera_blocked_;}
    bool battlefield_ready()const{return ready_&&(ready_key_>>48)==1;}
    int camera_x()const{return ready_camera_x_;}
    int camera_y()const{return ready_camera_y_;}
    int left()const{return ready_left_;}
    int top()const{return ready_top_;}
    uint64_t rendered=0,fallbacks=0;
    ~ThirdAgeView()override{
        // One report at destruction, never a per-frame disk flush.
        if(dither_stats_enabled_)std::fprintf(stderr,
            "[dither-oam] cache=%u calls=%llu hits=%llu misses=%llu key_bytes=%zu\n",
            unsigned(dither_cache_enabled_),static_cast<unsigned long long>(dither_calls_),
            static_cast<unsigned long long>(dither_hits_),
            static_cast<unsigned long long>(dither_calls_-dither_hits_),sizeof(dither_cache_));
        if(dither_timing_enabled_)std::fprintf(stderr,
            "[dither-oam-time] cache=%u calls=%llu total_ns=%llu\n",
            unsigned(dither_cache_enabled_),static_cast<unsigned long long>(dither_timed_calls_),
            static_cast<unsigned long long>(dither_ns_));
    }
    void bind(const uint8_t* iw,const uint8_t* ew,const uint8_t* rom,size_t size,ThirdAgeSettings* s){invalidate_dither_oam_cache();live_iw_=iw;live_ew_=ew;rom_=rom;rom_size_=size;settings_=s;}
    void restored()noexcept override{invalidate_dither_oam_cache();weather_frame_.clear();weather_candidate_=weather_valid_=false;motion_positions_.clear();motion_keep_=0;motion_frame_.clear();video_capture_.restored();capture_ppu_=nullptr;menu_fade_.clear();menu_mask_={};menu_tail_=0;pause_art_ready_=pause_art_frame_=false;hd_menu_={};menu_strings_.clear();command_ready_=command_complete_=command_capture_=false;hd_command_={};command_portraits_.clear();pending_health_={};native_left_ready_=native_left_complete_=false;corner_ready_=corner_complete_=corner_context_=false;reset_hd();idle_.clear();visual_ticks_.clear();ready_=drawing_=false;lines_=0;identity_=0;tiles_.clear();sprites_.clear();}
    void scanline(const gba::GbaPpu& ppu,uint32_t y,uint16_t display,const uint8_t* io,const uint8_t* vram,const uint8_t* oam,const uint8_t* pal)override{
        if(y==0)invalidate_dither_oam_cache(); // All frame/mode/early-return paths.
        if(ThirdAgeVideoCapture::enabled()){
            capture_ppu_=&ppu;
            video_capture_.scanline(ppu.frame_count(),y,display,live_iw_,live_ew_,io,vram,oam,pal,ppu.presentation_affine_state());
        }
        try{capture_command_underlay(ppu,y,display,io,vram,oam,pal);}catch(const Unsupported&){command_complete_=command_capture_=false;}
        try{capture_native_corner(ppu,y,display,io,vram,oam,pal);}
        catch(const Unsupported&){corner_complete_=corner_context_=false;}
        // The ROM's full-screen combat cut uses matching BG/OBJ mosaic.
        // Render a clear host canvas, then apply that grid once to the complete
        // expanded picture, including relocated UI. Guest IO stays untouched.
        std::array<uint8_t,1024> clear_io{};
        const unsigned mosaic=u16(io+0x4c);
        bool full_mosaic=mosaic&&((mosaic&255)==(mosaic>>8));
        for(unsigned bg=0;bg<4;++bg)
            if((display&(256u<<bg))&&!(u16(io+8+2*bg)&64))full_mosaic=false;
        if(y==0){mosaic_x_=full_mosaic?(mosaic&15)+1:1;mosaic_y_=full_mosaic?((mosaic>>4)&15)+1:1;}
        if(full_mosaic){std::memcpy(clear_io.data(),io,96);clear_io[0x4c]=clear_io[0x4d]=0;io=clear_io.data();}
        if(y==0){black_palette_=true;camera_backgrounds_=true;
            weather_rain_=weather_supported(display,io);
            weather_candidate_=weather_valid_=third_age_present_hz()&&live_iw_[0x4840]==1&&
                (weather_rain_||(dry_text_supported(display,io)&&has_screen_font(oam)));
            if(weather_candidate_){weather_mask_.fill(0);weather_foreground_.fill(0);}
        }
        // A screen-owned weather/parallax layer needs its own motion model.
        // Keep it faithful until that independent layer is implemented.
        if(live_iw_)for(unsigned bg=1;bg<4;++bg)
            if((display&(256u<<bg))&&!live_iw_[0x7080+bg]&&!(weather_candidate_&&weather_rain_&&bg==2))camera_backgrounds_=false;
        if(black_palette_){
            for(unsigned i=0;i<1024;i+=2)if(u16(pal+i)&0x7fff){black_palette_=false;break;}
            if((display&128)||((u16(io+0x50)&0xc0)==0x80&&(u16(io+0x50)&0x3f)&&u16(io+0x54)))black_palette_=false;
        }
        if(y==0){
            drawing_=false;combat_=false;stabilize_=false;lines_=0;motion_possible_=false;motion_actor_=0;
            if(!settings_||!settings_->expanded_view){menu_fade_.clear();menu_mask_={};menu_tail_=0;pause_art_ready_=pause_art_frame_=false;hd_menu_={};hd_command_={};reset_hd();idle_.clear();visual_ticks_.clear();return;}
            std::memcpy(iw_.data(),live_iw_,iw_.size());std::memcpy(ew_.data(),live_ew_,ew_.size());
            trace_effects(io);
            try{prepare_hd();}catch(const Unsupported&){reset_hd();}
            if(live_iw_[0x4840]==15||(live_iw_[0x4840]==2&&live_iw_[0x4860]==15)){hd_panel_={};hd_left_panel_={};}
            hd_menu_={};
            if(settings_->hd_battle_menus||settings_->ability_descriptions){
                try{hd_menu_=third_age_battle_menu([&](uint32_t a,unsigned n){return read(a,n);},menu_strings_);}catch(const Unsupported&){hd_menu_={};}
            }
            pause_art_frame_=settings_->hd_battle_menus&&hd_menu_.valid&&hd_menu_.pause;
            if(pause_art_frame_){
                // Keep authored scenery, faction emblems and ornamental frame;
                // only BG0's original lettering is replaced by host text.
                if(!pause_art_)pause_art_=std::make_unique<gba::GbaPpu>();
                // Pause transitions fade palette RAM, not only BLDCNT.
                // Palette entry 1 is the native white text entry.
                const unsigned white=u16(pal+2);
                hd_menu_.opacity=255*std::max({white&31,(white>>5)&31,(white>>10)&31})/31;
                const unsigned blend=u16(io+0x50),fade=std::min(16u,unsigned(u16(io+0x54)&31));
                if((blend&0xc0)==0xc0&&(blend&1))hd_menu_.opacity=hd_menu_.opacity*(16-fade)/16;
            }
            if(settings_->hd_battle_menus&&live_iw_[0x4840]==2&&live_iw_[0x4850]==3&&menu_mask_.valid){menu_tail_=3;}
            else if(settings_->hd_battle_menus&&hd_menu_.valid&&!hd_menu_.pause){menu_mask_=hd_menu_;menu_tail_=3;}
            else if(menu_tail_&&live_iw_[0x4840]==1)--menu_tail_;
            else {menu_mask_={};menu_tail_=0;}
            hd_menu_=menu_fade_.frame(hd_menu_,live_iw_[0x4840]==2&&(live_iw_[0x4860]==1||live_iw_[0x4860]==15));
            try{prepare_command();}catch(const Unsupported&){hd_command_={};}
            // Keep drawing the outgoing map while its transition owns the scene.
            const bool battle=live_iw_[0x4840]==1&&(live_iw_[0x4880]==1||live_iw_[0x4880]>=128);
            // Battle dialogs keep their map until the scene handoff completes.
            // Pending return (0x81) lasts multiple frames after confirmation
            // or cancel; dropping it here briefly exposed the native border.
            dialog_=live_iw_[0x4840]==2&&live_iw_[0x4860]==1&&
                (live_iw_[0x4880]==2||live_iw_[0x4880]==0x81);
            combat_=live_iw_[0x4840]==8;
            stabilize_=settings_->blend&&!combat_;
            if(!battle&&!dialog_&&!combat_){motion_positions_.clear();motion_keep_=0;idle_.clear();visual_ticks_.clear();if(pause_art_frame_)pause_art_->render_presentation_scanline(ppu,y,display&~256u,io,vram,oam,pal);return;}
            hud_.fill(HudPixel{});
            try{
                if(combat_){
                    if(display&7)return;
                    left_=40;top_=10;drawing_=true;
                }else{
                if(read(0x03003536,1)>1||(display&7))return;
                map_w_=read(0x03006d50,2);map_h_=read(0x0300701c,2);
                if(map_w_<8||map_w_>256||map_h_<6||map_h_>256)throw Unsupported{};
                const auto player=read(0x03003536,1);
                cx_=int32_t(read(0x03001bb0+player*8));cy_=int32_t(read(0x03001bb4+player*8));
                if(cx_<0||cy_<0||cx_>map_w_*32||cy_>map_h_*32)throw Unsupported{};
                const int dx=((int(u16(io+20))-cx_+128)&255)-128;
                const int dy=((int(u16(io+22))-cy_+128)&255)-128;
                if(std::abs(dx)>16||std::abs(dy)>16)return;
                // RAM can already contain the next camera step while OAM and
                // background scroll still display the preceding one. Clamp
                // the host view against that raster position, not the future
                // step; otherwise edge insets oscillate as IO catches up.
                cx_+=dx;cy_+=dy;
                uint32_t identity=2166136261u;
                for(unsigned i=0x6d70;i<0x6fe8;++i)identity=(identity^iw_[i])*16777619u;
                for(unsigned i=0x7060;i<0x70a0;++i)identity=(identity^iw_[i])*16777619u;
                if(identity!=identity_){tiles_.clear();identity_=identity;motion_positions_.clear();motion_keep_=0;}
                const int world_w=int(read(0x0300700c,2))*8-1;
                const int world_bottom=int(read(0x03007004,2))*8-65;
                left_=world_w<320?std::clamp(cx_+(320-world_w)/2,0,80):std::clamp(cx_-std::clamp(cx_-40,0,world_w-320),0,80);
                top_=std::clamp(cy_-std::clamp(cy_-10,32,std::max(32,world_bottom-180)),0,20);
                std::memcpy(vr_.data(),vram,vr_.size());prepare_objects(oam);prepare_terrain(io);drawing_=true;
                if(fixed_weather_needed()){
                    fixed_weather_mask_.fill(0);fixed_weather_rgb_.fill(0);
                    // Fixed-screen overscan is independent of the world inset.
                    // Extend the nearest captured raster row, as for the map.
                    for(int sy=-10;sy<0;++sy)fixed_weather_line(sy,io,pal,vram);
                }
                for(int sy=-top_;sy<0;++sy){line(sy,display,io,pal,vram);if(stabilize_)line(sy,display,io,pal,vram,true);
                    if(weather_candidate_){line(sy,display,io,pal,vram,false,false,true);if(stabilize_)line(sy,display,io,pal,vram,true,false,true);weather_line(sy,display,io,pal,vram,oam);}}
                if(motion_possible_)for(int sy=-top_;sy<0;++sy)line(sy,display,io,pal,vram,false,true);
                }
            }catch(const Unsupported&){++fallbacks;drawing_=false;}
        }
        if(pause_art_frame_&&pause_art_)pause_art_->render_presentation_scanline(ppu,y,display&~256u,io,vram,oam,pal);
        if(!drawing_)return;
        try{
            if(y!=lines_||(display&7))throw Unsupported{};
            if(!combat_){line(int(y),display,io,pal,vram);if(stabilize_)line(int(y),display,io,pal,vram,true);
                if(weather_candidate_){line(int(y),display,io,pal,vram,false,false,true);if(stabilize_)line(int(y),display,io,pal,vram,true,false,true);weather_line(int(y),display,io,pal,vram,oam);}}
            if(!combat_&&motion_possible_)line(int(y),display,io,pal,vram,false,true);
            if(!world_ppu_)world_ppu_=std::make_unique<gba::GbaPpu>();
            std::array<uint8_t,1024> world_palette{};
            std::memcpy(world_palette.data(),pal,world_palette.size());
            // The ROM uses magenta as its backdrop/debug color. Uncovered map
            // pixels at the wider boundary must use the canvas padding color.
            world_palette[0]=0x21;world_palette[1]=0x08;
            const unsigned hud_bg=combat_?2:0,hud_bit=256u<<hud_bg;
            if(stabilize_){
                std::array<uint8_t,1024> off{},on{};dither_oam(oam,off.data(),on.data());
                if(!world_on_)world_on_=std::make_unique<gba::GbaPpu>();
                world_ppu_->render_presentation_scanline(ppu,y,display&~hud_bit,io,vram,off.data(),world_palette.data());
                world_on_->render_presentation_scanline(ppu,y,display&~hud_bit,io,vram,on.data(),world_palette.data());
                if(weather_candidate_){
                    auto dry_off=off,dry_on=on;
                    for(unsigned i=0;i<128;++i)if(!(u16(oam+8*i+4)&0xc00)){dry_off[8*i+1]=(dry_off[8*i+1]&~1u)|2u;dry_on[8*i+1]=(dry_on[8*i+1]&~1u)|2u;}
                    if(!dry_ppu_)dry_ppu_=std::make_unique<gba::GbaPpu>();
                    if(!dry_on_ppu_)dry_on_ppu_=std::make_unique<gba::GbaPpu>();
                    dry_ppu_->render_presentation_scanline(ppu,y,display&~(hud_bit|(weather_rain_?1024u:0u)),io,vram,dry_off.data(),world_palette.data());
                    dry_on_ppu_->render_presentation_scanline(ppu,y,display&~(hud_bit|(weather_rain_?1024u:0u)),io,vram,dry_on.data(),world_palette.data());
                }
            }else {world_ppu_->render_presentation_scanline(ppu,y,display&~hud_bit,io,combat_?combat_vram(io,vram):vram,oam,world_palette.data());
                if(weather_candidate_){if(!dry_ppu_)dry_ppu_=std::make_unique<gba::GbaPpu>();
                    std::array<uint8_t,1024> dry_oam{};std::memcpy(dry_oam.data(),oam,1024);
                    for(unsigned i=0;i<128;++i)if(!(u16(oam+8*i+4)&0xc00))dry_oam[8*i+1]=(dry_oam[8*i+1]&~1u)|2u;
                    dry_ppu_->render_presentation_scanline(ppu,y,display&~(hud_bit|(weather_rain_?1024u:0u)),io,vram,dry_oam.data(),world_palette.data());}
            }
            if(weather_candidate_){
                // Retain native priority-zero object coverage above the rain.
                // White OBJ/black backdrop yields a binary mask with the PPU's
                // actual OAM, affine, transparency and tile addressing rules.
                if(!weather_front_ppu_)weather_front_ppu_=std::make_unique<gba::GbaPpu>();
                auto front_io=std::array<uint8_t,1024>{};auto front_oam=std::array<uint8_t,1024>{};auto front_pal=std::array<uint8_t,1024>{};
                std::memcpy(front_io.data(),io,96);std::memcpy(front_oam.data(),oam,1024);
                front_io[0x50]=front_io[0x51]=0;
                for(unsigned i=512;i<1024;i+=2){front_pal[i]=0xff;front_pal[i+1]=0x7f;}
                for(unsigned i=0;i<128;++i)if(u16(oam+8*i+4)&0xc00){front_oam[8*i+1]=(front_oam[8*i+1]&~1u)|2u;}
                weather_front_ppu_->render_presentation_scanline(ppu,y,display&~0xf00u,front_io.data(),vram,front_oam.data(),front_pal.data());
            }
            if(combat_){
                if(!combat_background_)combat_background_=std::make_unique<gba::GbaPpu>();
                // Extend only the authored backdrop. Do not repeat combatants,
                // projectiles, the platform, portraits, or combat text.
                combat_background_->render_presentation_scanline(ppu,y,display&~0x1e00u,io,vram,oam,world_palette.data());
            }
            if((display&hud_bit)&&!(display&128u))for(int x=0;x<240;++x){
                const unsigned index=raster_background(hud_bg,x,int(y),io,vram);
                if(index&&hud_visible(x,int(y),display,io,hud_bg))hud_[y*240+x]={uint16_t(u16(pal+2*index)),uint16_t((u16(io+0x50)&~1u)|((u16(io+0x50)>>hud_bg)&1)),uint16_t(u16(io+0x52)),uint16_t(u16(io+0x54)),true};
            }
            ++lines_;
            if(!combat_&&y==159)for(int sy=160;sy<180-top_;++sy){line(sy,display,io,pal,vram);if(stabilize_)line(sy,display,io,pal,vram,true);
                if(weather_candidate_){line(sy,display,io,pal,vram,false,false,true);if(stabilize_)line(sy,display,io,pal,vram,true,false,true);weather_line(sy,display,io,pal,vram,oam);}}
            if(!combat_&&y==159&&fixed_weather_needed())for(int sy=160;sy<170;++sy)fixed_weather_line(sy,io,pal,vram);
            if(!combat_&&y==159&&motion_possible_)for(int sy=160;sy<180-top_;++sy)line(sy,display,io,pal,vram,false,true);
        }catch(const Unsupported&){++fallbacks;drawing_=false;}
    }
    void frame_ready()override{
        invalidate_dither_oam_cache();
        motion_frame_.clear();motion_reason_=1;weather_frame_.clear();
        pause_art_ready_=pause_art_frame_;
        if(pause_art_ready_)pause_art_->mark_framebuffer_latched();
        command_ready_=command_complete_&&command_capture_;
        if(command_ready_)command_underlay_=command_underlay_work_;
        native_left_ready_=native_left_complete_&&corner_context_;
        if(native_left_ready_)native_left_=native_left_work_;
        corner_ready_=corner_complete_&&corner_context_;
        if(corner_ready_)native_corner_=native_corner_work_;
        ready_=drawing_&&lines_==160;
        if(ready_){
            world_ppu_->mark_framebuffer_latched();
            const auto* world=world_ppu_->latched_framebuffer();
            const uint8_t* on=nullptr,*dry=nullptr,*dry_on=nullptr;
            if(weather_candidate_){weather_front_ppu_->mark_framebuffer_latched();dry_ppu_->mark_framebuffer_latched();dry=dry_ppu_->latched_framebuffer();
                if(stabilize_){dry_on_ppu_->mark_framebuffer_latched();dry_on=dry_on_ppu_->latched_framebuffer();}}
            if(stabilize_){world_on_->mark_framebuffer_latched();on=world_on_->latched_framebuffer();}
            if(combat_){
                combat_background_->mark_framebuffer_latched();
                const auto* backdrop=combat_background_->latched_framebuffer();
                for(int y=0;y<180;++y)for(int x=0;x<320;++x){
                    int sx=x-40,sy=y-10;
                    // Reflect scenery at the original frame edges: no stretch
                    // or access to the unpopulated tilemap columns beyond 240.
                    sx=sx<0?-sx-1:sx>=240?479-sx:sx;
                    sy=sy<0?-sy-1:sy>=160?319-sy:sy;
                    std::memcpy(work_.data()+3*(y*320+x),backdrop+3*(sy*240+sx),3);
                }
            }
            for(int y=0;y<160;++y)for(int x=0;x<240;++x){
                auto* dst=work_.data()+3*((y+top_)*320+x+left_);
                const bool outside=!combat_&&(x+cx_<0||x+cx_>=int(read(0x0300700c,2))*8-1||y+cy_<32||y+cy_>=int(read(0x03007004,2))*8-65);
                if(outside){dst[0]=7;dst[1]=11;dst[2]=20;}
                else if(!objects_[(y+top_)*320+x+left_].repair||combat_)
                    std::memcpy(dst,world+3*(y*240+x),3);
                if(weather_candidate_){
                    const size_t wi=(y+top_)*320+x+left_;
                    if(!outside)weather_foreground_[wi]=weather_front_ppu_->latched_framebuffer()[3*(y*240+x)]!=0;
                    auto* d=dry_work_.data()+3*wi;
                    if(outside)std::memcpy(d,dst,3);
                    else if(!objects_[(y+top_)*320+x+left_].repair)std::memcpy(d,dry+3*(y*240+x),3);
                    if(stabilize_){auto* d_on=dry_on_.data()+3*((y+top_)*320+x+left_);
                        if(outside)std::memcpy(d_on,d,3);
                        else if(!objects_on_[(y+top_)*320+x+left_].repair)std::memcpy(d_on,dry_on+3*(y*240+x),3);}
                }
                if(stabilize_){
                    auto* other=work_on_.data()+3*((y+top_)*320+x+left_);
                    if(outside)std::memcpy(other,dst,3);
                    else if(!objects_on_[(y+top_)*320+x+left_].repair)
                        std::memcpy(other,on+3*(y*240+x),3);
                }
            }
            // Resolve the two authored visibility phases at this camera and
            // raster, before HUD relocation and the full-canvas combat mosaic.
            // This is spatial composition, so a history reset cannot flash a
            // fully opaque (or missing) border/turn marker for a single frame.
            if(stabilize_)for(size_t i=0;i<work_.size();++i)
                work_[i]=uint8_t((unsigned(work_[i])+work_on_[i]+1)/2);
            if(weather_candidate_){
                if(stabilize_)for(size_t i=0;i<dry_work_.size();++i)dry_work_[i]=uint8_t((unsigned(dry_work_[i])+dry_on_[i]+1)/2);
                for(int y=0;y<180;++y)for(int x=0;x<320;++x){
                    const int nx=x-left_,ny=y-top_;const auto& obj=objects_[y*320+x];
                    if((nx<0||nx>=240||ny<0||ny>=160||obj.repair)&&obj.palette&&obj.key<512)weather_foreground_[y*320+x]=1;
                }
                // Exact full-canvas reconstruction is a gate, not a visual
                // approximation. Keep the completed original on any mismatch.
                if(weather_valid_)for(size_t i=0;i<weather_mask_.size();++i)
                    if(std::memcmp(work_.data()+3*i,(weather_foreground_[i]?work_:weather_mask_[i]?weather_rgb_:dry_work_).data()+3*i,3)){weather_valid_=false;break;}
                if(weather_valid_){
                    // Only normalize AFTER the untouched native-space gate.
                    // The completed base frame and separate SDL weather plane
                    // must agree, including a camera upload/draw fallback.
                    const bool fixed=fixed_weather_needed();
                    const auto& rain_rgb=fixed?fixed_weather_rgb_:weather_rgb_;
                    const auto& rain_mask=fixed?fixed_weather_mask_:weather_mask_;
                    weather_frame_.rgb.assign(rain_rgb.begin(),rain_rgb.end());
                    weather_frame_.mask.assign(rain_mask.begin(),rain_mask.end());
                    // The native viewport slides inside the expanded map near
                    // boundaries. UI text belongs at the fixed 40,10 screen
                    // inset, never at that moving world inset or clipped to map.
                    for(size_t i=0;i<weather_foreground_.size();++i)if(fixed||weather_foreground_[i])
                        std::memcpy(work_.data()+3*i,(rain_mask[i]?rain_rgb:dry_work_).data()+3*i,3);
                    weather_frame_.foreground.assign(320*180,0);
                    weather_frame_.foreground_rgb.assign(320*180*3,0);
                    const auto* font=weather_front_ppu_->latched_framebuffer();
                    for(int y=0;y<160;++y)for(int x=0;x<240;++x){
                        const size_t src=y*240+x,dst=(y+10)*320+x+40;
                        if(!font[3*src])continue;
                        weather_frame_.foreground[dst]=1;
                        std::memcpy(weather_frame_.foreground_rgb.data()+3*dst,world+3*src,3);
                        std::memcpy(work_.data()+3*dst,world+3*src,3);
                    }
                }
                else camera_backgrounds_=false;
            }
            camera_blocked_=(!camera_backgrounds_?2:0)|(combat_?4:0)|(dialog_?8:0)|(black_palette_?16:0)|((mosaic_x_!=1||mosaic_y_!=1)?32:0);
            camera_safe_=third_age_present_hz()&&!camera_blocked_;
            camera_hud_.fill(0);
            if(camera_safe_)camera_world_=weather_frame_.valid()?dry_work_:work_;
            if(camera_safe_&&motion_possible_){
                bool exact=true;unsigned pixels=0;
                for(int y=motion_t_-8;y<motion_b_+8;++y)for(int x=motion_l_-8;x<motion_r_+8;++x){
                    const size_t i=y*320+x;if(!motion_sweep_[i])continue;const bool unit=motion_unit_[i].palette!=0&&!motion_foreground_[i];
                    const auto* expected=(unit?motion_expected_:motion_under_).data()+3*i;
                    if(std::memcmp(camera_world_.data()+3*i,expected,3))exact=false;
                    pixels+=unit;
                }
                motion_reason_=exact?0:4;
                if(exact&&pixels){
                    auto& out=motion_frame_;out.identity=(uint64_t(motion_actor_)<<24)|(uint64_t(read(motion_actor_+8,1))<<16)|read(motion_actor_+10,2);
                    out.world_x=int32_t(read(motion_actor_+32))/65536;out.world_y=int32_t(read(motion_actor_+36))/65536;
                    out.left=motion_l_;out.top=motion_t_;out.width=motion_r_-motion_l_;out.height=motion_b_-motion_t_;
                    out.background.assign(camera_world_.begin(),camera_world_.end());out.mask.assign(out.width*out.height,0);
                    out.sprite_rgb.assign(motion_expected_.begin(),motion_expected_.end());out.foreground.assign(motion_foreground_.begin(),motion_foreground_.end());
                    for(int y=motion_t_;y<motion_b_;++y)for(int x=motion_l_;x<motion_r_;++x){
                        const size_t i=y*320+x;if(!motion_unit_[i].palette)continue;
                        out.mask[(y-motion_t_)*out.width+x-motion_l_]=1;
                        if(!motion_foreground_[i])std::memcpy(out.background.data()+3*i,motion_under_.data()+3*i,3);
                    }
                }
            }else if(camera_safe_)motion_reason_=motion_actor_?motion_reject_:2;

            finish_hud();
            // Do not let decorative padding introduce color while the ROM's
            // entire palette is black during loading or a combat cut.
            if(black_palette_)work_.fill(0);
            if(mosaic_x_>1||mosaic_y_>1){
                for(unsigned y=0;y<180;++y)for(unsigned x=0;x<320;++x)
                    std::memcpy(latched_.data()+3*(y*320+x),work_.data()+3*((y-y%mosaic_y_)*320+x-x%mosaic_x_),3);
            }else latched_=work_;
            ready_left_=left_;ready_top_=top_;ready_camera_x_=cx_;ready_camera_y_=cy_;
            // A battlefield dialog overlays the same map. Keep the dither
            // history across its open/close handoff; camera moves and actual
            // scene changes still have distinct keys.
            ready_key_=(uint64_t(combat_?8:1)<<48)|(uint64_t(settings_->hd_unit_panel)<<47)|(uint64_t(settings_->hd_command_panel)<<46)|
                (combat_?0:(uint64_t(uint16_t(cx_-left_))<<16)|uint16_t(cy_-top_));
            camera_key_=(uint64_t(identity_)<<16)|(ready_key_&0xffff000000000000ull);
            ++rendered;
        }
        if(capture_ppu_&&video_capture_.active()){
            const uint32_t flags=settings_?(unsigned(settings_->expanded_view)|(unsigned(settings_->blend)<<1)|
                (unsigned(settings_->hd_unit_panel)<<2)|(unsigned(settings_->hd_command_panel)<<3)|
                (unsigned(settings_->hd_battle_menus)<<4)|(unsigned(settings_->hd_portraits)<<5)):0;
            video_capture_.frame_ready(capture_ppu_->latched_framebuffer(),ready_?latched_.data():nullptr,
                ready_key_,ready_left_,ready_top_,ready_camera_x_,ready_camera_y_,flags);
        }
        drawing_=false;
    }
    // Return the completed host presentation. No second simulation.
    const uint8_t* compose(const uint8_t* original){
        if(!settings_||!settings_->expanded_view)return nullptr;
        if(ready_){history_key_=ready_key_;return latched_.data();}
        if(!original)return nullptr;
        // Keep one canvas/scale through menus, unsupported effects and scene
        // changes. Native fallback never resizes the host surface mid-cut.
        const bool map=live_iw_&&(live_iw_[0x4840]==1||
            (live_iw_[0x4840]==2&&live_iw_[0x4860]==1));
        if(!map){ready_left_=40;ready_top_=10;}
        history_key_=(uint64_t(1)<<63)|(uint64_t(live_iw_?live_iw_[0x4840]:0)<<48)|(uint64_t(settings_->hd_unit_panel)<<47)|(uint64_t(settings_->hd_command_panel)<<46);
        fallback_.fill(0);
        if(pause_art_ready_&&settings_->hd_battle_menus){
            const auto* art=pause_art_->latched_framebuffer();
            for(int y=0;y<160;++y)std::memcpy(fallback_.data()+3*((y+10)*320+40),art+y*240*3,720);
            return fallback_.data();
        }
        for(int y=0;y<160;++y)std::memcpy(fallback_.data()+3*((y+ready_top_)*320+ready_left_),original+y*240*3,720);
        if(settings_->hd_command_panel&&hd_command_.valid&&command_ready_){
            for(unsigned y=0;y<48;++y)std::memcpy(fallback_.data()+3*((y+112+ready_top_)*320+ready_left_),command_underlay_.data()+216*y,216);
        }
        if(settings_->hd_unit_panel&&native_left_ready_){
            for(int y=0;y<40;++y)std::memcpy(fallback_.data()+3*((y+ready_top_)*320+ready_left_),native_left_.data()+y*120,120);
        }
        if(settings_->hd_unit_panel&&corner_ready_){
            for(unsigned y=0;y<40;++y)std::memcpy(fallback_.data()+3*((y+ready_top_)*320+200+ready_left_),native_corner_.data()+120*y,120);
        }
        return fallback_.data();
    }
};
}
