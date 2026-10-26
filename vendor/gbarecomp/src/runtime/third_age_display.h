#pragma once
#include "third_age_settings.h"
#include <cmath>
namespace gbarecomp {
// Window-only display transaction. Guest rendering and simulation are untouched.
struct ThirdAgeDisplay {
    struct Choice {
        bool fullscreen=false;int resolution=0,ui_scale=100;
        bool operator==(const Choice& b)const{return fullscreen==b.fullscreen&&resolution==b.resolution&&ui_scale==b.ui_scale;}
        bool operator!=(const Choice& b)const{return !(*this==b);}
    };
    Choice draft{},before{};
    bool pending=false;
    Uint64 deadline=0;
    SDL_Rect previous{};
    Uint32 previous_mode=0;bool previous_maximized=false;
    std::string status;
    static Choice get(const ThirdAgeSettings& s){return {s.fullscreen,s.resolution,s.ui_scale};}
    static void put(ThirdAgeSettings& s,const Choice& c){s.fullscreen=c.fullscreen;s.resolution=c.resolution;s.ui_scale=c.ui_scale;}
    static SDL_Point preset(int n){static constexpr SDL_Point sizes[]={{0,0},{1280,720},{1920,1080},{2560,1440},{3840,2160},{0,0}};return sizes[std::clamp(n,0,5)];}
    static std::string label(int n,int legacy){static const char* names[]={"","1280 X 720","1920 X 1080","2560 X 1440","3840 X 2160","DESKTOP"};return n==0?"LEGACY "+std::to_string(legacy)+"X":names[std::clamp(n,0,5)];}
    static float overlay_scale(int w,int h,int percent){return std::min(w/720.f,h/480.f)*std::min(1.f,.75f*percent/100.f);}
    void edit(const ThirdAgeSettings& s){if(!pending)draft=get(s);}
    bool configure(const Choice& c,const ThirdAgeSettings& s,SDL_Window* w){
        const Uint32 wanted=c.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0;
        const Uint32 mode=SDL_GetWindowFlags(w)&SDL_WINDOW_FULLSCREEN_DESKTOP;
        if(mode!=wanted&&SDL_SetWindowFullscreen(w,wanted)!=0){status="DISPLAY CHANGE FAILED. PREVIOUS MODE RESTORED.";return false;}
        if(!c.fullscreen){
            SDL_Rect area{};const int display=std::max(0,SDL_GetWindowDisplayIndex(w));
            const bool bounds=SDL_GetDisplayUsableBounds(display,&area)==0&&area.w>0&&area.h>0;
            int top=0,left=0,bottom=0,right=0;SDL_GetWindowBordersSize(w,&top,&left,&bottom,&right);
            const int aw=bounds?std::max(1,area.w-left-right):0,ah=bounds?std::max(1,area.h-top-bottom):0;
            SDL_Point size=preset(c.resolution);
            if(c.resolution==0)size={240*s.scale,160*s.scale};
            if(c.resolution==5){if(!bounds){status="DESKTOP SIZE UNAVAILABLE. PREVIOUS MODE RESTORED.";return false;}size={aw,ah};}
            bool fitted=false;
            if(bounds&&(size.x>aw||size.y>ah)){
                const double fit=std::min(double(aw)/size.x,double(ah)/size.y);
                size={std::max(1,int(std::floor(size.x*fit))),std::max(1,int(std::floor(size.y*fit)))};fitted=true;
            }
            SDL_RestoreWindow(w);SDL_SetWindowSize(w,size.x,size.y);
            SDL_SetWindowPosition(w,SDL_WINDOWPOS_CENTERED_DISPLAY(display),SDL_WINDOWPOS_CENTERED_DISPLAY(display));
            status=fitted?"WINDOW FITTED TO AVAILABLE DESKTOP SPACE.":"";
        } else status="BORDERLESS USES YOUR DESKTOP RESOLUTION.";
        return true;
    }
    void startup(ThirdAgeSettings& s,SDL_Window* w){
        draft=get(s);
        if(!configure(draft,s,w)){
            SDL_SetWindowFullscreen(w,0);SDL_SetWindowSize(w,720,480);
            s.fullscreen=false;s.resolution=0;draft=get(s);
        }
    }
    bool apply(ThirdAgeSettings& s,SDL_Window* w,Uint64 now=SDL_GetTicks64()){
        if(pending||draft==get(s))return false;
        before=get(s);SDL_GetWindowPosition(w,&previous.x,&previous.y);SDL_GetWindowSize(w,&previous.w,&previous.h);
        previous_mode=SDL_GetWindowFlags(w)&SDL_WINDOW_FULLSCREEN_DESKTOP;
        previous_maximized=(SDL_GetWindowFlags(w)&SDL_WINDOW_MAXIMIZED)!=0;
        pending=true;deadline=now+15000;
        if(!configure(draft,s,w)){const auto error=status;revert(s,w);status=error;return false;}
        put(s,draft);return true; // Unsaved until Keep; a crash retains the last confirmed file.
    }
    bool revert(ThirdAgeSettings& s,SDL_Window* w){
        if(!pending)return false;
        const bool ok=SDL_SetWindowFullscreen(w,0)==0;
        SDL_RestoreWindow(w);SDL_SetWindowSize(w,previous.w,previous.h);SDL_SetWindowPosition(w,previous.x,previous.y);
        if(previous_maximized)SDL_MaximizeWindow(w);
        const bool restored=previous_mode==0||SDL_SetWindowFullscreen(w,previous_mode)==0;
        put(s,before);draft=before;pending=false;deadline=0;
        if(!ok||!restored)s.fullscreen=(SDL_GetWindowFlags(w)&SDL_WINDOW_FULLSCREEN)!=0;
        status=ok&&restored?"PREVIOUS DISPLAY SETTINGS RESTORED.":"RESTORE FAILED. USE YOUR WINDOW CONTROLS.";
        return true;
    }
    void keep(ThirdAgeSettings& s){if(!pending)return;pending=false;deadline=0;s.save();status="DISPLAY SETTINGS KEPT.";}
    bool tick(ThirdAgeSettings& s,SDL_Window* w,Uint64 now=SDL_GetTicks64()){return pending&&now>=deadline?revert(s,w):false;}
    int seconds(Uint64 now=SDL_GetTicks64())const{return pending&&deadline>now?int((deadline-now+999)/1000):0;}
};
}
