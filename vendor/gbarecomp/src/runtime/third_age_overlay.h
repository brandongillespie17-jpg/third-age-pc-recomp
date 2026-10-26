#pragma once
#include <cstring>
#include <cstdio>
#include "third_age_settings.h"
#include "third_age_display.h"
#include "third_age_hd_panel.h"
// Small SDL-only host UI: no guest framebuffer changes or added dependencies.
namespace gbarecomp {
struct ThirdAgeOverlay {
    ThirdAgeOverlay()=default;
    ThirdAgeOverlay(const ThirdAgeOverlay&)=delete;
    ThirdAgeOverlay& operator=(const ThirdAgeOverlay&)=delete;
    ~ThirdAgeOverlay(){panel_preview_.release();if(canvas_renderer_)SDL_DestroyRenderer(canvas_renderer_);if(canvas_)SDL_FreeSurface(canvas_);}
    mutable SDL_Surface* canvas_=nullptr;
    mutable SDL_Renderer* canvas_renderer_=nullptr;
    mutable bool draw_error_reported_=false,draw_info_reported_=false;
    ThirdAgeSettings* settings=nullptr;
    ThirdAgeDisplay display;
    bool confirm_keep=false;
    bool open=false,release_block=false;
    int tab=0,row=0,hover=-1;
    int pointer_x=-1,pointer_y=-1;
    bool panel_colors=false,advanced_video=false;
    int color_part=0,color_row=0,drag_channel=-1;
    mutable ThirdAgeHdPanel panel_preview_;
    void finish_color_drag(){if(drag_channel>=0&&settings)settings->save();drag_channel=-1;}
    int channel(int i)const{return (settings->panel_theme.colors[color_part]>>(16-8*i))&255;}
    void set_channel(int i,int value){auto& c=settings->panel_theme.colors[color_part];const int shift=16-8*i;c=(c&~(255u<<shift))|(uint32_t(std::clamp(value,0,255))<<shift);}
    void color_change(int direction){
        if(color_row==0)color_part=(color_part+ThirdAgePanelTheme::Count+direction)%ThirdAgePanelTheme::Count;
        else if(color_row<=3){set_channel(color_row-1,channel(color_row-1)+direction);settings->save();}
        else if(color_row==4){settings->panel_theme.colors[color_part]=ThirdAgePanelTheme::defaults[color_part];settings->save();}
        else if(color_row==5){settings->panel_theme=ThirdAgePanelTheme{};settings->save();}
        else if(direction>0)exit_game();
    }
    static bool hit(int x,int y,int l,int t,int w,int h){return x>=l&&y>=t&&x<l+w&&y<t+h;}
    int rows()const{return tab==2?2:tab==1?(advanced_video?10:2):tab==3?7:tab==4?6:5;}
    int row_step()const{return tab==1?22:24;}
    int row_height()const{return tab==1?20:22;}
    void exit_game() {
        // Use the same event as closing the window: the runtime owns save
        // flushing, audio shutdown and window teardown, including while paused.
        SDL_Event quit{};quit.type=SDL_QUIT;
        if(SDL_PushEvent(&quit)!=1)std::fprintf(stderr,"third-age-overlay: could not request exit: %s\n",SDL_GetError());
    }
    bool enabled(int r)const{
        if(r==rows())return false; // Exit is an action, not an adjustable setting.
        if(tab==0 && r>0 && r<4)return settings->mouse&&(r==1||settings->edge_camera);
        if(tab==1&&!advanced_video&&r==1)return settings->hd_enabled;
        if(tab==1&&advanced_video&&!settings->hd_enabled)return false;
        if(tab==1 && advanced_video && r==8 && third_age_original_portraits())return false;
        if(tab==4 && r==1)return !display.draft.fullscreen;
        if(tab==4 && r==5)return settings->camera_smoothing;
        if(tab==4 && r==3)return display.draft!=ThirdAgeDisplay::get(*settings);
        return true;
    }
    void change(int direction,SDL_Window* window=nullptr) {
        if(!settings||!enabled(row))return;
        auto& s=*settings;
        if(tab==0){if(row==0)s.mouse=!s.mouse;else if(row==1)s.edge_camera=!s.edge_camera;
            else if(row==2)s.outside_scroll=!s.outside_scroll;else if(row==3)s.edge_speed=std::clamp(s.edge_speed+direction,1,6);else s.native_ai=!s.native_ai;}
        if(tab==1&&!advanced_video){
            if(row==0)s.set_hd_enabled(!s.hd_enabled);
            else{advanced_video=true;row=0;return;}
            s.save();return;
        }
        if(tab==1){if(row==0)s.blend=!s.blend;else if(row==1)s.smooth=!s.smooth;
            else if(row==2)s.fps=!s.fps;else if(row==3)s.expanded_view=!s.expanded_view;else if(row==4)s.hd_unit_panel=!s.hd_unit_panel;else if(row==5)s.hd_command_panel=!s.hd_command_panel;else if(row==6)s.hd_battle_menus=!s.hd_battle_menus;else if(row==7)s.ability_descriptions=!s.ability_descriptions;else if(row==8)s.hd_portraits=!s.hd_portraits;else{panel_colors=true;color_row=0;return;}}
        if(tab==2){if(row==0)s.volume=std::clamp(s.volume+5*direction,0,100);else s.mute=!s.mute;}
        if(tab==3){if(row==0)s.skip_combat=!s.skip_combat;else if(row==1)s.native_graphics=!s.native_graphics;else if(row==2)s.fast_path=!s.fast_path;
            else if(row==3)s.combat_skip_button=!s.combat_skip_button;else if(row==4)s.damage_numbers=!s.damage_numbers;else if(row==5)s.fast_combat_skip=!s.fast_combat_skip;else if(row==6)s.sprite_cache=!s.sprite_cache;}
        if(tab==4){
            if(row==0)display.draft.fullscreen=!display.draft.fullscreen;
            else if(row==1)display.draft.resolution=std::clamp(display.draft.resolution+direction,0,5);
            else if(row==2)display.draft.ui_scale=std::clamp(display.draft.ui_scale+25*direction,75,150);
            else if(row==3&&window){confirm_keep=false;display.apply(s,window);}
            else if(row==4){s.camera_smoothing=!s.camera_smoothing;s.save();}
            else if(row==5){
                static constexpr int rates[]={60,120,144,165};int index=0;
                while(index<3&&rates[index]!=s.presentation_hz)++index;
                s.presentation_hz=rates[std::clamp(index+direction,0,3)];s.save();
            }
            return;
        }
        s.save();
    }
    bool input_event(Uint32 type)const{
        return type==SDL_KEYDOWN||type==SDL_KEYUP||type==SDL_MOUSEMOTION||type==SDL_MOUSEBUTTONDOWN||type==SDL_MOUSEBUTTONUP||type==SDL_MOUSEWHEEL||type==SDL_CONTROLLERBUTTONDOWN||type==SDL_CONTROLLERBUTTONUP||type==SDL_CONTROLLERAXISMOTION;
    }
    bool event(const SDL_Event& e,SDL_Renderer* renderer,SDL_Window* window) {
        if(!settings)return false;
        auto point=[&](int& x,int& y){
            x=e.type==SDL_MOUSEMOTION?e.motion.x:e.button.x;
            y=e.type==SDL_MOUSEMOTION?e.motion.y:e.button.y;
            int lw=0,lh=0;SDL_RenderGetLogicalSize(renderer,&lw,&lh);
            SDL_Rect vp{};SDL_RenderGetViewport(renderer,&vp);
            if(!lw||!lh){int ww=0,wh=0,dw=0,dh=0;SDL_GetWindowSize(window,&ww,&wh);SDL_GetRendererOutputSize(renderer,&dw,&dh);
                if(ww<=0||wh<=0)return false;
                x=int(float(x)*dw/ww)-vp.x;y=int(float(y)*dh/wh)-vp.y;
            }
            const float scale=ThirdAgeDisplay::overlay_scale(vp.w,vp.h,settings->ui_scale);
            if(scale<=0)return false;
            x=int(std::floor((x-(vp.w-720*scale)/2)/scale));y=int(std::floor((y-(vp.h-480*scale)/2)/scale));return true;
        };
        if(e.type==SDL_MOUSEMOTION||e.type==SDL_MOUSEBUTTONDOWN){
            if(!point(pointer_x,pointer_y))pointer_x=pointer_y=-1;
        }else if(e.type==SDL_KEYDOWN||(e.type==SDL_WINDOWEVENT&&
            (e.window.event==SDL_WINDOWEVENT_LEAVE||e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)))pointer_x=pointer_y=-1;
        if(display.pending){
            if(e.type==SDL_KEYDOWN&&!e.key.repeat){
                const auto k=e.key.keysym.sym;
                if(k==SDLK_ESCAPE||k==SDLK_F10)display.revert(*settings,window);
                else if(k==SDLK_LEFT||k==SDLK_RIGHT||k==SDLK_TAB)confirm_keep=!confirm_keep;
                else if(k==SDLK_RETURN||k==SDLK_SPACE){if(confirm_keep)display.keep(*settings);else display.revert(*settings,window);}
            }
            if(e.type==SDL_MOUSEMOTION||e.type==SDL_MOUSEBUTTONDOWN){int x,y;if(point(x,y)){
                const bool keep=hit(x,y,120,268,220,40),back=hit(x,y,380,268,220,40);
                if(keep||back)confirm_keep=keep;
                if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT){if(keep)display.keep(*settings);else if(back)display.revert(*settings,window);}
            }}
            return input_event(e.type);
        }
        if(e.type==SDL_KEYDOWN&&!e.key.repeat&&(e.key.keysym.sym==SDLK_F10||e.key.keysym.sym==SDLK_ESCAPE)){
            finish_color_drag();
            if(panel_colors&&open&&e.key.keysym.sym==SDLK_ESCAPE){panel_colors=false;return true;}
            if(tab==1&&advanced_video&&open&&e.key.keysym.sym==SDLK_ESCAPE){advanced_video=false;row=1;return true;}
            panel_colors=false;
            open=!open;if(open)display.edit(*settings);release_block=true;hover=-1;return true;
        }
        if(!open)return release_block&&input_event(e.type);
        if(panel_colors){
            if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)finish_color_drag();
            if(e.type==SDL_KEYDOWN){const auto key=e.key.keysym.sym;
                if(key==SDLK_UP)color_row=(color_row+6)%7;
                else if(key==SDLK_DOWN||key==SDLK_TAB)color_row=(color_row+1)%7;
                else if(key==SDLK_LEFT)color_change(-1);
                else if(key==SDLK_RIGHT||key==SDLK_RETURN||key==SDLK_SPACE)color_change(1);
            }
            if(e.type==SDL_MOUSEWHEEL){if(e.wheel.y)color_change(e.wheel.y>0?1:-1);}
            if(e.type==SDL_MOUSEBUTTONUP&&e.button.button==SDL_BUTTON_LEFT)finish_color_drag();
            if(e.type==SDL_MOUSEMOTION||e.type==SDL_MOUSEBUTTONDOWN){int x,y;if(!point(x,y))return true;
                const bool click=e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT;
                if(drag_channel>=0&&e.type==SDL_MOUSEMOTION){set_channel(drag_channel,int(std::lround((x-110)*255./198)));return true;}
                if(click&&hit(x,y,528,402,132,34)){finish_color_drag();panel_colors=false;open=false;release_block=true;return true;}
                if(click&&hit(x,y,528,55,132,32)){finish_color_drag();exit_game();return true;}
                for(int t=0;t<5;++t)if(click&&hit(x,y,60+t*120,104,112,38)){finish_color_drag();panel_colors=false;tab=t;row=0;return true;}
                if(hit(x,y,60,158,600,34)){color_row=0;if(click)color_change(x<360?-1:1);}
                for(int i=0;i<3;++i)if(hit(x,y,60,202+40*i,306,34)){
                    color_row=i+1;if(click){drag_channel=i;set_channel(i,int(std::lround((x-110)*255./198)));}
                }
                if(hit(x,y,60,340,290,34)){color_row=4;if(click)color_change(1);}
                if(hit(x,y,370,340,290,34)){color_row=5;if(click)color_change(1);}
            }
            return input_event(e.type);
        }
        if(e.type==SDL_KEYDOWN&&!e.key.repeat){
            const auto key=e.key.keysym.sym;
            if(key==SDLK_TAB){tab=(tab+((e.key.keysym.mod&KMOD_SHIFT)?4:1))%5;row=0;}
            else if(key==SDLK_UP)row=(row+rows())%(rows()+1);
            else if(key==SDLK_DOWN)row=(row+1)%(rows()+1);
            else if((key==SDLK_RETURN||key==SDLK_SPACE)&&row==rows())exit_game();
            else if(key==SDLK_LEFT)change(-1,window);
            else if(key==SDLK_RIGHT||key==SDLK_RETURN||key==SDLK_SPACE)change(1,window);
        }
        if(e.type==SDL_MOUSEMOTION||e.type==SDL_MOUSEBUTTONDOWN){
            int x,y;if(!point(x,y))return true;
            const bool click=e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT;hover=-1;
            if(hit(x,y,528,55,132,32)){hover=rows();row=rows();if(click)exit_game();return true;}
            if(hit(x,y,528,402,132,34)&&click){if(tab==1&&advanced_video){advanced_video=false;row=1;return true;}open=false;release_block=true;return true;}
            for(int t=0;t<5;++t)if(hit(x,y,60+t*120,104,112,38)&&click){tab=t;row=0;}
            for(int r=0;r<rows();++r)if(hit(x,y,60,158+r*row_step(),600,row_height())){
                hover=r;row=r;if(click)change(x<530?-1:1,window);
            }
        }
        return input_event(e.type);
    }
    static void text(SDL_Renderer* r,float x,float y,const std::string& value,double size,SDL_Color color) {
        ThirdAgeHdPanel::host_text(r,x,y,value,size,color);
    }
    void draw(SDL_Renderer* r,SDL_Window* window=nullptr)const{
        if(!open||!settings){draw_info_reported_=false;return;}
        auto failed=[&](const char* operation){
            if(!draw_error_reported_)std::fprintf(stderr,"third-age-overlay: %s failed: %s\n",operation,SDL_GetError());
            draw_error_reported_=true;
        };
        // Rasterize on an independent CPU canvas. Never change the game
        // renderer's logical size, viewport, scale, clip or render target.
        SDL_Rect vp{};SDL_RenderGetViewport(r,&vp);
        const float scale=ThirdAgeDisplay::overlay_scale(vp.w,vp.h,settings->ui_scale);
        float sx=1,sy=1;SDL_RenderGetScale(r,&sx,&sy);
        const float density=std::clamp(scale*std::max(sx,sy),1.f,8.f);
        const int cw=int(std::ceil(720*density)),ch=int(std::ceil(480*density));
        if(canvas_&&(canvas_->w!=cw||canvas_->h!=ch)){
            panel_preview_.release();SDL_DestroyRenderer(canvas_renderer_);canvas_renderer_=nullptr;
            SDL_FreeSurface(canvas_);canvas_=nullptr;
        }
        if(!canvas_)canvas_=SDL_CreateRGBSurfaceWithFormat(0,cw,ch,32,SDL_PIXELFORMAT_ARGB8888);
        if(!canvas_){failed("canvas allocation");return;}
        if(!canvas_renderer_)canvas_renderer_=SDL_CreateSoftwareRenderer(canvas_);
        if(!canvas_renderer_){failed("canvas renderer");return;}
        SDL_RenderSetScale(canvas_renderer_,cw/720.f,ch/480.f);
        SDL_SetRenderDrawBlendMode(canvas_renderer_,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(canvas_renderer_,0,0,0,0);SDL_RenderClear(canvas_renderer_);
        SDL_SetRenderDrawBlendMode(canvas_renderer_,SDL_BLENDMODE_BLEND);
        draw_contents(canvas_renderer_,window,r);SDL_RenderPresent(canvas_renderer_);
        SDL_Texture* texture=SDL_CreateTextureFromSurface(r,canvas_);
        if(!texture){failed("canvas upload");return;}
        SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
        SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);
#endif
        const SDL_FRect dst{(vp.w-720*scale)/2,(vp.h-480*scale)/2,720*scale,480*scale};
        if(!draw_info_reported_){
            SDL_RendererInfo info{};SDL_GetRendererInfo(r,&info);SDL_version version;SDL_GetVersion(&version);
            int lw=0,lh=0;SDL_RenderGetLogicalSize(r,&lw,&lh);
            std::fprintf(stderr,"third-age-overlay: canvas=%dx%d SDL=%u.%u.%u video=%s renderer=%s logical=%dx%d viewport=%d,%d %dx%d\n",
                cw,ch,version.major,version.minor,version.patch,SDL_GetCurrentVideoDriver()?SDL_GetCurrentVideoDriver():"unknown",
                info.name?info.name:"unknown",lw,lh,vp.x,vp.y,vp.w,vp.h);
            draw_info_reported_=true;
        }
        if(scale>0 && SDL_RenderCopyF(r,texture,nullptr,&dst)!=0)failed("canvas draw");
        // Destroy flushes the queued copy while this texture is still valid.
        // No renderer-owned texture survives resize/reset or window teardown.
        SDL_DestroyTexture(texture);
    }
    static constexpr double title_font=30,row_font=19,tab_font=18,note_font=11;
    static void label(SDL_Renderer* r,int x,int y,int w,int h,const std::string& value,double font,int align,SDL_Color color){
        ThirdAgeHdPanel::host_label(r,SDL_Rect{x,y,w,h},value,font,align,color);
    }
    bool hot(int x,int y,int w,int h)const{return open&&hit(pointer_x,pointer_y,x,y,w,h);}
    void button(SDL_Renderer* r,int x,int y,int w,int h,const std::string& value,double font,bool selected=false,bool danger=false)const{
        const bool over=hot(x,y,w,h);const SDL_Color gold{255,214,82,255},white{232,235,230,255};
        SDL_Color bg=danger?(over?SDL_Color{135,65,47,255}:SDL_Color{68,39,35,255}):
            over?SDL_Color{66,83,59,255}:selected?SDL_Color{54,73,63,255}:SDL_Color{28,40,38,255};
        SDL_SetRenderDrawColor(r,bg.r,bg.g,bg.b,bg.a);SDL_Rect rect{x,y,w,h};SDL_RenderFillRect(r,&rect);
        if(over){SDL_SetRenderDrawColor(r,195,159,74,255);SDL_RenderDrawRect(r,&rect);}
        label(r,x,y,w,h,value,font,1,over||selected?gold:white);
    }
    void footer(SDL_Renderer* r,const std::string& hint,const std::string& status)const{
        const SDL_Color gold{222,191,113,255},muted{155,168,161,255};
        label(r,74,380,572,20,hint,note_font,0,muted);
        label(r,74,403,438,16,status,note_font,0,settings->save_failed?SDL_Color{255,140,120,255}:gold);
        label(r,74,421,438,16,panel_colors?"F10: RESUME   ESC: BACK   ARROWS: ADJUST":"F10 / ESC: RESUME   TAB: CATEGORY   ARROWS: ADJUST",note_font,0,muted);
        button(r,528,402,132,34,tab==1&&advanced_video&&!panel_colors?"BACK":"RESUME",row_font);
    }
    void draw_panel_colors(SDL_Renderer* r)const{
        panel_preview_.set_hd_portraits(settings->hd_portraits);
        const auto& theme=settings->panel_theme;
        auto color=[](uint32_t c){return SDL_Color{Uint8(c>>16),Uint8(c>>8),Uint8(c),255};};
        auto box=[&](int x,int y,int w,int h,SDL_Color c){SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);SDL_Rect q{x,y,w,h};SDL_RenderFillRect(r,&q);};
        const SDL_Color gold{222,191,113,255},white{232,235,230,255},muted{155,168,161,255};
        auto field=[&](int x,int y,int w,int h,int index){box(x,y,w,h,hot(x,y,w,h)?SDL_Color{66,83,59,255}:color_row==index?SDL_Color{40,58,51,255}:SDL_Color{24,35,33,255});};
        field(60,158,600,34,0);button(r,66,158,32,34,"-",row_font);
        label(r,110,158,500,34,ThirdAgePanelTheme::labels[color_part],row_font,1,hot(60,158,600,34)?SDL_Color{255,214,82,255}:white);button(r,622,158,32,34,"+",row_font);
        const char* channels[]={"R","G","B"};
        for(int i=0;i<3;++i){const int y=202+40*i;field(60,y,306,34,i+1);label(r,74,y,24,34,channels[i],row_font,0,white);
            for(int x=0;x<=198;++x){auto c=theme.colors[color_part];const int shift=16-8*i;c=(c&~(255u<<shift))|(uint32_t(x*255/198)<<shift);box(110+x,y+10,1,14,color(c));}
            const int handle=110+int(std::lround(channel(i)*198./255));box(handle-2,y+7,5,20,white);box(handle-1,y+9,3,16,{20,25,23,255});
            label(r,318,y,36,34,std::to_string(channel(i)),row_font,2,gold);
        }
        label(r,380,198,264,20,"LIVE PREVIEW",note_font,1,muted);
        ThirdAgeUnitPanel p;p.valid=true;p.name="Unit preview";p.hp=color_part==ThirdAgePanelTheme::LowHealthFill?5:12;p.max_hp=20;p.attack=6;p.range=4;p.movement=3;
        if(color_part>=ThirdAgePanelTheme::SpiritText){
            ThirdAgeCommandPanel c;c.valid=true;c.name="Commander";c.spirit=8;c.max_spirit=20;c.turns=3;
            if(color_part>=ThirdAgePanelTheme::FreeMoveOutline)c.turn_style=ThirdAgeTurnStyle(1+color_part-ThirdAgePanelTheme::FreeMoveOutline);
            panel_preview_.command_preview(r,c,SDL_Rect{440,222,144,84},theme);
        }else panel_preview_.preview(r,p,SDL_Rect{380,226,264,67},theme);
        box(380,310,60,24,color(theme.colors[color_part]));label(r,454,310,190,24,ThirdAgePanelTheme::hex(theme.colors[color_part]),row_font,0,white);
        button(r,60,340,290,34,"RESET THIS COLOR",row_font,color_row==4);
        button(r,370,340,290,34,"RESET ALL COLORS",row_font,color_row==5);
        footer(r,color_row==6?"CLOSE THE GAME WINDOW.":"DRAG RGB SLIDERS. ARROWS ADJUST. ESC RETURNS TO VIDEO.",settings->save_failed?"COULD NOT SAVE SETTINGS.":"COLORS SAVE AUTOMATICALLY.");
    }
    void draw_contents(SDL_Renderer* r,SDL_Window* window=nullptr,SDL_Renderer* output=nullptr)const{
        auto box=[&](int x,int y,int w,int h,SDL_Color c){SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);SDL_Rect q{x,y,w,h};SDL_RenderFillRect(r,&q);};
        const SDL_Color gold{222,191,113,255},white{232,235,230,255},muted{155,168,161,255};
        box(0,0,720,480,{0,0,0,185});box(40,38,640,406,{18,27,27,255});box(40,38,640,3,gold);
        label(r,60,50,470,34,"THIRD AGE PC",title_font,0,gold);label(r,60,84,470,18,"GAME PAUSED / SETTINGS",note_font,0,muted);
        const bool exit_selected=panel_colors?color_row==6:row==rows();
        button(r,528,55,132,32,"EXIT GAME",row_font,exit_selected,true);
        const char* tabs[]={"CONTROLS","VIDEO","AUDIO","GAMEPLAY","DISPLAY"};
        for(int t=0;t<5;++t)button(r,60+120*t,104,112,38,tabs[t],tab_font,t==tab);
        if(panel_colors){draw_panel_colors(r);return;}
        static const char* labels[5][10]={{"MOUSE CONTROLS","EDGE-ONLY CAMERA","OUTSIDE-WINDOW SCROLL","EDGE SCROLL SPEED","NATIVE AI LOOKUP"},{"DISPLAY BLENDING","SMOOTH SCALING","FPS IN TITLE BAR","EXPANDED BATTLEFIELD","UNIT PANEL","COMMANDER PANEL","BATTLEFIELD MENUS","ABILITY DESCRIPTIONS","HD PORTRAITS","HD PANEL COLORS"},{"VOLUME","MUTE"},{"AUTO-SKIP COMBAT","FAST GRAPHICS DECODING","FAST PATHFINDING","PRESS SPACE TO SKIP","SKIPPED COMBAT RESULTS","FAST COMBAT RESOLUTION","SPRITE CACHE"},{"WINDOW MODE","WINDOW RESOLUTION","OVERLAY SIZE","APPLY DISPLAY SETTINGS","CAMERA SMOOTHING","PRESENTATION RATE"}};
        const auto& s=*settings;
        std::string values[5][10]={{s.mouse?"ON":"OFF",s.edge_camera?"ON":"OFF",s.outside_scroll?"ON":"OFF",std::to_string(s.edge_speed),s.native_ai?"ON":"OFF"},
          {s.blend?"ON":"OFF",s.smooth?"ON":"OFF",s.fps?"ON":"OFF",s.expanded_view?"ON":"OFF",s.hd_unit_panel?"HD":"ORIGINAL",s.hd_command_panel?"HD":"ORIGINAL",s.hd_battle_menus?"HD":"ORIGINAL",s.ability_descriptions?"ON":"OFF",third_age_original_portraits()?"FORCED OFF":s.hd_portraits?"ON":"OFF","EDIT"},
          {std::to_string(s.volume),s.mute?"ON":"OFF"},{s.skip_combat?"ON":"OFF",s.native_graphics?"ON":"OFF",s.fast_path?"ON":"OFF",s.combat_skip_button?"ON":"OFF",s.damage_numbers?"ON":"OFF",s.fast_combat_skip?"ON":"OFF",s.sprite_cache?"ON":"OFF"},{display.draft.fullscreen?"BORDERLESS":"WINDOWED",display.draft.fullscreen?"DESKTOP":ThirdAgeDisplay::label(display.draft.resolution,s.scale),std::to_string(display.draft.ui_scale)+"%","APPLY",s.camera_smoothing?"ON":"OFF",std::to_string(s.presentation_hz)+" HZ"}};
        if(tab==1&&!advanced_video){
            values[1][0]=s.hd_enabled?"ON":"OFF";values[1][1]="OPEN";
        }
        for(int i=0;i<rows();++i){const bool en=enabled(i),over=en&&hot(60,158+i*row_step(),600,row_height());box(60,158+i*row_step(),600,row_height(),over?SDL_Color{66,83,59,255}:i==row?SDL_Color{40,58,51,255}:SDL_Color{24,35,33,255});
            label(r,74,158+i*row_step(),318,row_height(),(tab==1&&!advanced_video?(i==0?"HD PRESET":"ADVANCED"):labels[tab][i]),row_font,0,over?SDL_Color{255,214,82,255}:en?white:muted);
            const bool number=(tab==0&&i==3)||(tab==2&&i==0)||(tab==4&&(i<3||i==5));
            const int y=158+i*row_step();
            if(number){
                if(en)button(r,402,y,28,row_height(),"-",row_font);else label(r,402,y,28,row_height(),"-",row_font,1,muted);
                label(r,436,y,184,row_height(),values[tab][i],row_font,1,over?SDL_Color{255,214,82,255}:en?gold:muted);
                if(en)button(r,622,y,28,row_height(),"+",row_font);else label(r,622,y,28,row_height(),"+",row_font,1,muted);
            }else label(r,430,y,216,row_height(),values[tab][i],row_font,2,over?SDL_Color{255,214,82,255}:en?gold:muted);
        }
        static const char* hints[5][10]={{"HOVER AND CLICK MENUS AND BATTLEFIELD.","OFF RESTORES THE ORIGINAL CURSOR-FOLLOW CAMERA.","CONTINUE PANNING PAST THE WINDOW EDGE WHILE FOCUSED.","CAMERA PAN SPEED. GAMEPLAY SPEED STAYS NORMAL.","EXPERIMENTAL: FASTER AI NODE SEARCH. OFF USES ORIGINAL."},{"SMOOTHS FLICKERING MARKERS. MAY SOFTEN MOVING SPRITES.","OFF KEEPS PIXELS SHARP. THIS DOES NOT REPLACE ARTWORK.","SHOW PRESENTATION RATE IN THE WINDOW TITLE.","EXPANDED 320 X 180 CANVAS. GAMEPLAY SPEED IS UNCHANGED.","HD UNIT CARDS. REQUIRES EXPANDED BATTLEFIELD.","HD PORTRAIT, SPIRIT AND TURN POINTS. EXPANDED VIEW REQUIRED.","HD ITEMS, ABILITIES, PROMPTS AND TUTORIAL TEXT.","SHOW THE SELECTED MOVE OR ITEM DESCRIPTION IN A SIDE PANEL.","SWITCH APPROVED HD PORTRAITS ON OR OFF. APPLIES IMMEDIATELY.","CUSTOMIZE PANEL, TEXT, ICON AND HEALTH BAR COLORS."},{"HOST AUDIO LEVEL. GAME TIMING IS UNCHANGED.","SILENCE AUDIO WITHOUT CHANGING YOUR VOLUME SETTING."},{"RESOLVE COMBAT OFFSCREEN. A SHORT PAUSE MAY REMAIN.","EXPERIMENTAL: DECODE ORIGINAL ARTWORK FASTER.","EXPERIMENTAL: BATCH ORIGINAL PATH SEARCH CALCULATIONS.","SPACE SKIPS THIS FIGHT. TURN AUTO-SKIP OFF TO WATCH FIRST.","SHOW HP LOST AND CONFIRMED MISSES AFTER SKIPPED COMBAT.","SHORTEN HIDDEN ANIMATION WAITS WHEN SKIPPING A FIGHT.","EXPERIMENTAL: REUSE MATCHING SPRITE DATA TO REDUCE RENDER WORK."},{"BORDERLESS USES DESKTOP RESOLUTION. WINDOWED USES PRESET.","LARGE WINDOWS FIT YOUR WORK AREA. ARTWORK STAYS ORIGINAL.","RESIZE THIS OVERLAY WITHOUT CHANGING THE GAME HUD.","PREVIEW FOR 15 SECONDS. KEEP OR AUTOMATICALLY REVERT.","SMOOTHER CAMERA MOTION IN EXPANDED BATTLEFIELD. APPLIES NOW.","MATCH YOUR DISPLAY REFRESH RATE. GAMEPLAY SPEED STAYS NORMAL."}};
        if(tab==4){
            if(window&&output){int ww=0,wh=0,dw=0,dh=0;SDL_GetWindowSize(window,&ww,&wh);SDL_GetRendererOutputSize(output,&dw,&dh);
                label(r,74,336,572,20,"WINDOW "+std::to_string(ww)+" X "+std::to_string(wh)+" / OUTPUT "+std::to_string(dw)+" X "+std::to_string(dh),note_font,0,white);
            }
            label(r,74,358,572,20,display.status,note_font,0,gold);
        }
        footer(r,row==rows()?"CLOSE THE GAME WINDOW.":tab==1&&advanced_video&&row==8&&third_age_original_portraits()?"REMOVE THE ORIGINAL-PORTRAITS LAUNCH OPTION TO ENABLE HD.":tab==1&&!advanced_video?(row==0?"TOGGLE HD VISUALS. YOUR ADVANCED CHOICES ARE REMEMBERED.":"TWEAK INDIVIDUAL HD SETTINGS. ENABLE HD TO EDIT."):hints[tab][row],s.save_failed?"COULD NOT SAVE SETTINGS. CHECK FOLDER PERMISSIONS.":(tab==4&&row<4?"DISPLAY CHANGES SAVE ONLY AFTER KEEP.":"PREFERENCES SAVE AUTOMATICALLY."));
        if(display.pending){
            box(40,38,640,406,{10,16,16,240});box(80,164,560,168,{24,35,33,255});
            label(r,104,177,512,30,"KEEP THESE DISPLAY SETTINGS?",23,1,gold);
            label(r,104,212,512,28,"REVERTING IN "+std::to_string(display.seconds())+" SECONDS",row_font,1,white);
            label(r,104,239,512,21,"ESC REVERTS. ARROWS SELECT. ENTER CONFIRMS.",note_font,1,muted);
            button(r,120,268,220,40,"KEEP",row_font,confirm_keep);
            button(r,380,268,220,40,"REVERT",row_font,!confirm_keep);
        }
    }
};
}
