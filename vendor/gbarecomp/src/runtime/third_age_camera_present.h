#pragma once
#include "third_age_refresh_clock.h"
#include "third_age_unit_motion.h"
#include "third_age_weather.h"
#include <array>
#include <vector>
namespace gbarecomp {
// Completed-world camera presentation with verified, isolated unit cutouts.
// Pose selection and gameplay remain native; only position is interpolated.
class ThirdAgeCameraPresent {
    SDL_Texture *current_=nullptr,*previous_=nullptr,*older_=nullptr,*hud_=nullptr;
    int previous_x_=0,previous_y_=0,older_x_=0,older_y_=0;unsigned history_=0;
    std::vector<uint8_t> world_,hud_rgb_;
    std::vector<uint32_t> hud_pixels_;
    ThirdAgeUnitMotionFrame unit_;
    ThirdAgeWeatherFrame weather_;
    SDL_Texture *weather_texture_=nullptr,*weather_front_texture_=nullptr;bool weather_uploaded_=false;
    SDL_Texture *unit_texture_=nullptr,*foreground_texture_=nullptr;
    bool unit_uploaded_=false;
    ThirdAgeScrollMotion scroll_;bool mouse_owned_=false,edge_panning_=false;
    double drawn_dx_=0,drawn_dy_=0;
    bool ready_=false,pending_=false,uploaded_=false;
    uint64_t next_frame_=0,next_key_=0;int next_x_=0,next_y_=0;
public:
    // Diagnostic values never feed back into camera or draw state. Displacements
    // are captured from the actual destinations, before subsequent tween reads.
    struct Diagnostics {
        bool enabled=false;
        unsigned upload_errors=0,draw_errors=0,reset_flags=0;
        int64_t uploaded_ns=0;
        double raw_dx=0,raw_dy=0,unit_raw_dx=0,unit_raw_dy=0;
        double unit_snapped_dx=0,unit_snapped_dy=0,unit_total_dx=0,unit_total_dy=0;
    } diagnostics;
    unsigned history_depth()const{return history_;}
    bool mouse_owned()const{return mouse_owned_;}
    bool edge_panning()const{return edge_panning_;}
    const ThirdAgeScrollMotion& scroll_motion()const{return scroll_;}
    int unit_target_x()const{return unit_.world_x;}
    int unit_target_y()const{return unit_.world_y;}
    // Preserve SDL calls, ordering and short circuits while observing failures.
    int diagnostic_result(int result,unsigned bit,bool upload=false){
        if(diagnostics.enabled&&result){if(upload)diagnostics.upload_errors|=bit;else diagnostics.draw_errors|=bit;}
        return result;
    }
    ThirdAgeCameraTween tween;
    ThirdAgeUnitMotionTween unit_tween;
    bool weather_available()const{return weather_uploaded_;}
    bool unit_available()const{return unit_uploaded_;}
    bool available()const{return ready_&&uploaded_;}
    void set_scroll_input(bool owned,bool edge){mouse_owned_=owned;edge_panning_=edge;}
    bool continuous_scroll()const{return scroll_.active;}
    double drawn_dx()const{return drawn_dx_;}
    double drawn_dy()const{return drawn_dy_;}
    void release(){if(diagnostics.enabled)diagnostics.reset_flags|=64;scroll_.reset();drawn_dx_=drawn_dy_=0;

        for(auto* t:{current_,previous_,older_,hud_,unit_texture_,foreground_texture_,weather_texture_,weather_front_texture_})if(t)SDL_DestroyTexture(t);
        weather_texture_=weather_front_texture_=nullptr;weather_uploaded_=false;weather_.clear();
        unit_texture_=foreground_texture_=nullptr;unit_uploaded_=false;unit_.clear();unit_tween.reset();
        current_=previous_=older_=hud_=nullptr;history_=0;ready_=pending_=uploaded_=false;tween.reset();
    }
    void submit(const uint8_t* world,const uint8_t* composite,const uint8_t* mask,uint64_t frame,uint64_t key,int x,int y,const ThirdAgeUnitMotionFrame* unit=nullptr,const ThirdAgeWeatherFrame* weather=nullptr){
        if(!world||!composite){if(diagnostics.enabled){diagnostics.reset_flags=1;diagnostics.upload_errors=0;}scroll_.reset();drawn_dx_=drawn_dy_=0;weather_.clear();weather_uploaded_=false;unit_uploaded_=false;unit_.clear();unit_tween.reset();ready_=pending_=uploaded_=false;history_=0;tween.reset();return;}
        if(ready_&&next_frame_==frame&&next_key_==key&&next_x_==x&&next_y_==y&&
           world_.size()==320*180*3&&std::equal(world_.begin(),world_.end(),world)&&std::equal(hud_rgb_.begin(),hud_rgb_.end(),composite))return;
        if(weather&&weather->valid())weather_=*weather;else weather_.clear();
        if(unit&&unit->valid())unit_=*unit;else unit_.clear();
        world_.assign(world,world+320*180*3);hud_rgb_.assign(composite,composite+320*180*3);
        hud_pixels_.resize(320*180);
        for(size_t p=0;p<hud_pixels_.size();++p){
            hud_pixels_[p]=mask&&mask[p]?0xff000000u:0;
        }
        next_frame_=frame;next_key_=key;next_x_=x;next_y_=y;ready_=pending_=true;
    }
    template<class Grade> void upload(SDL_Renderer* r,Grade grade,int64_t now=third_age_present_now()){
        if(!pending_||!ready_)return;
        pending_=false;
        if(diagnostics.enabled){diagnostics.upload_errors=diagnostics.reset_flags=0;}
        if(!current_){
            current_=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,320,180);
            previous_=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,320,180);
            older_=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,320,180);
            hud_=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,180);
            if(!current_||!previous_||!older_||!hud_){if(diagnostics.enabled)diagnostics.upload_errors|=1;release();return;}
            SDL_SetTextureBlendMode(hud_,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
            SDL_SetTextureScaleMode(current_,SDL_ScaleModeNearest);
            SDL_SetTextureScaleMode(previous_,SDL_ScaleModeNearest);
            SDL_SetTextureScaleMode(older_,SDL_ScaleModeNearest);
            SDL_SetTextureScaleMode(hud_,SDL_ScaleModeNearest);
#endif
        }
        const bool same_camera=tween.valid&&next_key_==tween.key&&next_x_==tween.x&&next_y_==tween.y;
        const int prior_x=tween.x,prior_y=tween.y;
        if(diagnostics.enabled&&tween.valid){
            if(next_frame_==tween.frame)diagnostics.reset_flags|=2;
            if(next_key_!=tween.key)diagnostics.reset_flags|=4;
        }
        if(tween.valid&&next_frame_==tween.frame)tween.reset();
        if(scroll_.active)scroll_.advance(tween.x,tween.y,now,edge_panning_);
        scroll_.observe_frame(next_frame_,now);
        tween.submit(next_frame_,next_key_,next_x_,next_y_,now);
        if(diagnostics.enabled){
            if(tween.reason==2)diagnostics.reset_flags|=8;
            if(tween.reason==3)diagnostics.reset_flags|=16;
            if(tween.reason==4)diagnostics.reset_flags|=32;
        }
        if(tween.reason==1||tween.reason==2||tween.reason==4)scroll_.reset();
        // Timestamp native target changes at upload, not at the next draw.
        if(scroll_.active)scroll_.advance(tween.x,tween.y,now,edge_panning_);

        // The previous texture must stay at the previous CAMERA position,
        // even when another native frame arrives at the current position.
        if(!same_camera){
            std::swap(older_,previous_);std::swap(previous_,current_);
            older_x_=previous_x_;older_y_=previous_y_;
            previous_x_=prior_x;previous_y_=prior_y;
            history_=std::min(2u,history_+1);
        }
        if(!tween.smooth&&!scroll_.active)history_=0;
        auto graded_world=world_,graded_hud=hud_rgb_;
        grade(world_.data(),graded_world.data());grade(hud_rgb_.data(),graded_hud.data());
        const bool had_weather=weather_uploaded_;weather_uploaded_=false;
        if(weather_.valid()){
            if(!weather_texture_){weather_texture_=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,180);
                if(weather_texture_){SDL_SetTextureBlendMode(weather_texture_,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
                    SDL_SetTextureScaleMode(weather_texture_,SDL_ScaleModeNearest);
#endif
                }
            }
            if(!weather_front_texture_){weather_front_texture_=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,180);
                if(weather_front_texture_){SDL_SetTextureBlendMode(weather_front_texture_,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
                    SDL_SetTextureScaleMode(weather_front_texture_,SDL_ScaleModeNearest);
#endif
                }
            }
            if(diagnostics.enabled&&(!weather_texture_||!weather_front_texture_))diagnostics.upload_errors|=2;
            if(weather_texture_&&weather_front_texture_){
                auto rgb=weather_.rgb;grade(weather_.rgb.data(),rgb.data());std::vector<uint32_t> pixels(320*180,0);
                for(size_t i=0;i<pixels.size();++i)if(weather_.mask[i])pixels[i]=0xff000000u|(uint32_t(rgb[3*i])<<16)|(uint32_t(rgb[3*i+1])<<8)|rgb[3*i+2];
                weather_uploaded_=diagnostic_result(SDL_UpdateTexture(weather_texture_,nullptr,pixels.data(),320*4),4,true)==0;
                std::fill(pixels.begin(),pixels.end(),0);
                if(weather_.foreground.size()==pixels.size()&&weather_.foreground_rgb.size()==rgb.size()){
                    grade(weather_.foreground_rgb.data(),rgb.data());
                    for(size_t i=0;i<pixels.size();++i)if(weather_.foreground[i])pixels[i]=0xff000000u|(uint32_t(rgb[3*i])<<16)|(uint32_t(rgb[3*i+1])<<8)|rgb[3*i+2];
                }
                weather_uploaded_=diagnostic_result(SDL_UpdateTexture(weather_front_texture_,nullptr,pixels.data(),320*4),8,true)==0&&weather_uploaded_;
            }
            // Missing weather must fall back to the original complete frame.
            if(!weather_uploaded_){if(diagnostics.enabled)diagnostics.reset_flags|=128;ready_=uploaded_=false;tween.reset();return;}
        }
        if(had_weather!=weather_uploaded_)history_=0;
        const bool had_unit=unit_uploaded_;
        unit_uploaded_=false;
        if(unit_.valid()){
            if(!unit_texture_){
                unit_texture_=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,180);
                
                if(unit_texture_){SDL_SetTextureBlendMode(unit_texture_,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
                    SDL_SetTextureScaleMode(unit_texture_,SDL_ScaleModeNearest);
#endif
                }
            }
            if(!foreground_texture_){
                foreground_texture_=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,180);
                if(foreground_texture_){SDL_SetTextureBlendMode(foreground_texture_,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
                    SDL_SetTextureScaleMode(foreground_texture_,SDL_ScaleModeNearest);
#endif
                }
            }
            if(diagnostics.enabled&&(!unit_texture_||!foreground_texture_))diagnostics.upload_errors|=16;
            if(unit_texture_&&foreground_texture_){
                auto sprite_rgb=graded_world;
                if(unit_.sprite_rgb.size()==sprite_rgb.size())grade(unit_.sprite_rgb.data(),sprite_rgb.data());
                std::vector<uint32_t> pixels(320*180,0);
                for(int y=0;y<unit_.height;++y)for(int x=0;x<unit_.width;++x){
                    const size_t j=y*unit_.width+x;if(!unit_.mask[j])continue;
                    const size_t i=3*((y+unit_.top)*320+x+unit_.left);
                    pixels[i/3]=0xff000000u|(uint32_t(sprite_rgb[i])<<16)|(uint32_t(sprite_rgb[i+1])<<8)|sprite_rgb[i+2];
                }
                unit_uploaded_=diagnostic_result(SDL_UpdateTexture(unit_texture_,nullptr,pixels.data(),320*4),32,true)==0;
                std::fill(pixels.begin(),pixels.end(),0);
                if(unit_.foreground.size()==pixels.size())for(size_t j=0;j<pixels.size();++j)if(unit_.foreground[j]){
                    const size_t i=j*3;pixels[j]=0xff000000u|(uint32_t(graded_world[i])<<16)|(uint32_t(graded_world[i+1])<<8)|graded_world[i+2];
                }
                unit_uploaded_=diagnostic_result(SDL_UpdateTexture(foreground_texture_,nullptr,pixels.data(),320*4),64,true)==0&&unit_uploaded_;
            }
            // Do not erase the native actor until its replacement is ready.
            if(unit_uploaded_)grade(unit_.background.data(),graded_world.data());
        }
        if(had_unit!=unit_uploaded_)history_=0;
        if(unit_uploaded_)unit_tween.submit(next_frame_,next_key_,unit_,now);else unit_tween.reset();

        for(size_t p=0;p<hud_pixels_.size();++p)if(hud_pixels_[p]){
            const size_t o=3*p;hud_pixels_[p]=0xff000000u|(uint32_t(graded_hud[o])<<16)|(uint32_t(graded_hud[o+1])<<8)|graded_hud[o+2];
        }
        if(diagnostic_result(SDL_UpdateTexture(current_,nullptr,graded_world.data(),320*3),128,true)||
           diagnostic_result(SDL_UpdateTexture(hud_,nullptr,hud_pixels_.data(),320*4),256,true)){if(diagnostics.enabled)diagnostics.reset_flags|=256;ready_=uploaded_=false;tween.reset();return;}
        uploaded_=true;
        if(diagnostics.enabled)diagnostics.uploaded_ns=third_age_present_now();
    }
    bool draw(SDL_Renderer* r,SDL_Rect rect,int64_t now=third_age_present_now()){
        if(diagnostics.enabled)diagnostics.draw_errors=0;
        if(!ready_||!uploaded_||rect.w<=0||rect.h<=0){if(diagnostics.enabled)diagnostics.draw_errors|=!ready_||!uploaded_?1:2;return false;}
#if SDL_VERSION_ATLEAST(2,0,10)
        SDL_Rect old_clip{};const bool had_clip=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&old_clip);
        SDL_Rect clip=rect;if(had_clip&&!SDL_IntersectRect(&old_clip,&rect,&clip)){if(diagnostics.enabled)diagnostics.draw_errors|=4;return false;}
        SDL_RenderSetClipRect(r,&clip);
        if(mouse_owned_){
            if(!scroll_.active)scroll_.start(tween.x-tween.offset_x(now),tween.y-tween.offset_y(now),now);
            scroll_.advance(tween.x,tween.y,now,edge_panning_);
        }else if(scroll_.active){
            // Ownership handoffs resume the existing native camera tween from
            // the actual displayed position, without carrying mouse velocity.
            tween.from_x=scroll_.x;tween.from_y=scroll_.y;tween.stamp=now;
            tween.duration_ns=ThirdAgeCameraTween::guest_ns;tween.smooth=true;
            scroll_.reset();
        }
        const double dx=scroll_.active?tween.x-scroll_.x:tween.offset_x(now);
        const double dy=scroll_.active?tween.y-scroll_.y:tween.offset_y(now);
        if(diagnostics.enabled){diagnostics.raw_dx=dx;diagnostics.raw_dy=dy;diagnostics.unit_raw_dx=diagnostics.unit_raw_dy=diagnostics.unit_snapped_dx=diagnostics.unit_snapped_dy=diagnostics.unit_total_dx=diagnostics.unit_total_dy=0;}
        drawn_dx_=std::round(dx*rect.w/320)*320./rect.w;
        drawn_dy_=std::round(dy*rect.h/180)*180./rect.h;

        const double sx=double(rect.w)/320,sy=double(rect.h)/180;
        // An unfinished tween can still expose the image TWO camera steps
        // back. Keep that image for edge coverage while retargeting continuously.
        // Completed-image backing covers diagonal corners outside all snapshots.
        if(dx!=0.||dy!=0.)diagnostic_result(SDL_RenderCopy(r,current_,nullptr,&rect),8);
        if((tween.smooth||scroll_.active)&&(dx!=0.||dy!=0.)){
            if(history_>=2){
                SDL_FRect older{float(rect.x+std::round((dx+older_x_-tween.x)*sx)),float(rect.y+std::round((dy+older_y_-tween.y)*sy)),float(rect.w),float(rect.h)};
                diagnostic_result(SDL_RenderCopyF(r,older_,nullptr,&older),16);
            }
            if(history_>=1){
                SDL_FRect prev{float(rect.x+std::round((dx+previous_x_-tween.x)*sx)),float(rect.y+std::round((dy+previous_y_-tween.y)*sy)),float(rect.w),float(rect.h)};
                diagnostic_result(SDL_RenderCopyF(r,previous_,nullptr,&prev),32);
            }
        }
        SDL_FRect next{float(rect.x+std::round(dx*sx)),float(rect.y+std::round(dy*sy)),float(rect.w),float(rect.h)};
        bool ok=diagnostic_result(SDL_RenderCopyF(r,current_,nullptr,&next),64)==0;
        if(unit_uploaded_){
            // Same camera displacement as the world; unit motion is applied
            // separately and snapped to output pixels to preserve sharp art.
            SDL_FRect sprite{float(rect.x+std::round(dx*sx)+std::round(unit_tween.x(now)*sx)),
                float(rect.y+std::round(dy*sy)+std::round(unit_tween.y(now)*sy)),float(rect.w),float(rect.h)};
            if(diagnostics.enabled){
                diagnostics.unit_raw_dx=unit_tween.x(now);diagnostics.unit_raw_dy=unit_tween.y(now);
                diagnostics.unit_snapped_dx=(sprite.x-next.x)/sx;diagnostics.unit_snapped_dy=(sprite.y-next.y)/sy;
                diagnostics.unit_total_dx=(sprite.x-rect.x)/sx;diagnostics.unit_total_dy=(sprite.y-rect.y)/sy;
            }
            ok=diagnostic_result(SDL_RenderCopyF(r,unit_texture_,nullptr,&sprite),128)==0&&ok;
            ok=diagnostic_result(SDL_RenderCopyF(r,foreground_texture_,nullptr,&next),256)==0&&ok;
        }
        if(weather_uploaded_){ok=diagnostic_result(SDL_RenderCopy(r,weather_texture_,nullptr,&rect),512)==0&&ok;
            ok=diagnostic_result(SDL_RenderCopy(r,weather_front_texture_,nullptr,&rect),1024)==0&&ok;}
        ok=diagnostic_result(SDL_RenderCopy(r,hud_,nullptr,&rect),2048)==0&&ok;
        SDL_RenderSetClipRect(r,had_clip?&old_clip:nullptr);return ok;
#else
        if(diagnostics.enabled)diagnostics.draw_errors|=4096;
        return false;
#endif
    }
};
}
