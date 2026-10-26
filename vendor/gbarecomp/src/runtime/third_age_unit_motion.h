#pragma once
#include "third_age_refresh_clock.h"
#include <vector>
namespace gbarecomp {
inline bool third_age_unit_motion_enabled(){
    static const bool enabled=[](){const char* p=std::getenv("THIRD_AGE_UNIT_MOTION");return !p||p[0]!='0';}();return enabled;
}
// Completed-frame, read-only sprite cutout. Alpha is a binary visibility mask;
// colors come from the verified native world, after host color grading.
struct ThirdAgeUnitMotionFrame {
    uint64_t identity=0;
    int world_x=0,world_y=0,left=0,top=0,width=0,height=0;
    std::vector<uint8_t> background,mask,sprite_rgb,foreground;
    bool valid()const{return identity&&width>0&&height>0&&background.size()==320*180*3&&mask.size()==size_t(width*height);}
    void clear(){identity=0;mask.clear();background.clear();sprite_rgb.clear();foreground.clear();}
};
// Same continuity rules as the camera, with unit identity added. Poses and
// lifetime remain native; only translation between completed positions moves.
class ThirdAgeUnitMotionTween {
    ThirdAgeCameraTween tween_;
    uint64_t identity_=0;
public:
    void reset(){tween_.reset();identity_=0;}
    void submit(uint64_t frame,uint64_t scene,const ThirdAgeUnitMotionFrame& p,int64_t now){
        if(!p.valid()){reset();return;}
        if(identity_!=p.identity)tween_.reset();
        identity_=p.identity;tween_.submit(frame,scene,p.world_x,p.world_y,now);
    }
    double x(int64_t now)const{return std::clamp(-tween_.offset_x(now),-8.,8.);}
    double y(int64_t now)const{return std::clamp(-tween_.offset_y(now),-8.,8.);}
    bool smooth()const{return tween_.smooth;}
    unsigned reason()const{return tween_.reason;}
    double alpha(int64_t now)const{return tween_.alpha(now);}
    double duration_ns()const{return tween_.duration_ns;}
};
}
