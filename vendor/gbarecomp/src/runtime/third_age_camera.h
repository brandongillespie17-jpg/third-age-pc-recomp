#pragma once
// Native selection cursor shapes: 24 for a large unit (Mumakil), 25 for
// one tile. Mask 0x7e accepts both, ignoring the native high flag bit.
// These are visual shapes, not different input/cursor object types.
// Mouse-owned camera for the exact Third Age ROM guarded by runtime.cpp.
// Redirect only the normal follow routine's target, never renderer reads.
// Map streaming, scripted camera modes and action validation stay game-owned.
// Neither ROM bytes nor guest RAM/save data are modified by this adapter.
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include "../gba/gba_bus.h"
#include "runtime_arm.h"
namespace gbarecomp {
class ThirdAgeCamera {
    const uint8_t* iw_=nullptr;
    inline static ThirdAgeCamera* active_=nullptr;
    RuntimeBusReadOverride previous_=nullptr;
    const uint8_t* ew_=nullptr;
    bool expanded_=false,pointer_valid_=false;int view_left_=0,view_top_=0;
    bool wanted_=false,locked_=false;
    int speed_=3;
    int px_=-1,py_=-1,cx_=0,cy_=0;
    unsigned player_=0;
    uint32_t cursor_=0;
    uint64_t last_frame_=0;
    bool have_center_=false;
    int last_cx_=0,last_cy_=0;
    uint32_t focus_unit_=0;
    unsigned focus_action_=0;bool focus_selected_=false;
    uint64_t focus_start_=0,focus_stable_=0;
    int focus_x_=0,focus_y_=0;
    bool focus_pointer_valid_=false;int focus_pointer_x_=0,focus_pointer_y_=0;
    uint32_t read(uint32_t a,unsigned n=4) const {
        const uint8_t* p=nullptr;
        if(a>=0x03000000&&a+n<=0x03008000)p=iw_+a-0x03000000;
        else if(a>=0x02000000&&a+n<=0x02040000)p=ew_+a-0x02000000;
        if(!p)return 0;
        uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(p[i])<<(8*i);return v;
    }
    bool eligible() const {
        if(!iw_||!ew_||read(0x03004840)!=1||read(0x03003536,1)!=player_)return false;
        if(player_>=2||cursor_<0x02000000||cursor_>0x0203fec0||(cursor_&3))return false;
        if(read(0x03003a90+16*player_)!=cursor_)return false;
        const auto action=read(cursor_+77,1);
        if((read(cursor_+12,1)&0x7e)!=24)return false;
        // Native target cycling and deployment own their camera.
        if(action!=17&&action!=18&&action!=19&&action!=20)return false;
        // Same turn-input gate as ROM routine 0800AEF4. AI and turn transitions
        // must own their camera, even when the last human cursor remains live.
        const auto ai=read(0x03007434);
        if(ai<0x02000000||ai>0x0203fa00)return false;
        // ROM 080399C8/08039A04/08039A18 expose AI side, enabled, active.
        if(read(ai+0x574,1)!=0 || (read(ai+0x576,1)!=0 &&
            read(ai+0x579,1)==read(0x0300352e + player_,1)))return false;
        const auto turn=read(0x03003534,1),side=read(0x03003533,1);
        if(turn==1 || (turn==2&&side!=2) || (turn==0&&side==2))return false;
        const auto world=read(0x03004898);
        if(world<0x02000000||world>0x0203fc00)return false;
        const auto phase=read(world+0x177+0xdc*player_,1);
        if(phase>=1&&phase<=4)return false; // automatic turn setup
        const uint32_t camera=0x030015b0+64*player_;
        const bool following_cursor=read(camera)==cursor_;
        return following_cursor && read(camera+56,2)==0 &&
            read(0x03003724+40*player_,1)==0 && !(read(0x030047f8)&0x810);
    }
    static int override_read(uint32_t pc,uint32_t a,uint32_t width,uint32_t original,uint32_t* value) {
        auto* self=active_;
        if(self && width==4 && (pc==0x0800d2ae || pc==0x0800d2b0) && self->locked_) {
            if(!self->eligible())self->release();
            else if(a==self->cursor_+(pc==0x0800d2ae?28:24)) {
                if(pc==0x0800d2ae){
                    const uint32_t base=0x030015b0+64*self->player_;
                    const int x=int32_t(self->read(base+4))/65536;
                    const int y=int32_t(self->read(base+8))/65536;
                    // A game-authored jump outranks mouse ownership. Normal
                    // panning is bounded to two guest frames at the chosen speed.
                    const int pan_limit=std::max(8,2*self->speed_+2);
                    if(self->have_center_ && (std::abs(x-self->last_cx_)>pan_limit || std::abs(y-self->last_cy_)>pan_limit)){
                        self->release();return 0;
                    }
                    self->last_cx_=x;self->last_cy_=y;self->have_center_=true;
                }
                *value=uint32_t(pc==0x0800d2ae?self->cy_+93:self->cx_+120)*65536;
                return 1;
            }
        }
        return self && self->previous_ ? self->previous_(pc,a,width,original,value) : 0;
    }

public:
    void bind(gba::GbaBus& bus) {
        iw_=bus.iwram_ptr();ew_=bus.ewram_ptr();
        previous_=g_runtime_bus_read_override;active_=this;
        g_runtime_bus_read_override=&override_read;
    }
    ~ThirdAgeCamera(){
        if(active_==this){
            if(g_runtime_bus_read_override==&override_read)g_runtime_bus_read_override=previous_;
            active_=nullptr;
        }
    }
    void set_expanded(bool value,int left=40,int top=10){
        if(value!=expanded_){release();expanded_=value;}
        const int next_left=value?left:0,next_top=value?top:0;
        // Reproject an existing pointer; layout changes are not fresh user
        // movement and must not reacquire a scripted/native camera.
        if(pointer_valid_){px_+=view_left_-next_left;py_+=view_top_-next_top;}
        if(focus_pointer_valid_){focus_pointer_x_+=view_left_-next_left;focus_pointer_y_+=view_top_-next_top;}
        view_left_=next_left;view_top_=next_top;
    }
    void set_speed(int speed){speed_=std::clamp(speed,1,6);}
    void release(){pointer_valid_=false;focus_unit_=0;have_center_=false;wanted_=locked_=false;px_=py_=-1;last_frame_=0;}
    bool mouse_owned()const{return locked_&&eligible();}
    bool edge_panning()const{
        if(!mouse_owned()||!pointer_valid_)return false;
        return px_<(expanded_?6-view_left_:6)||px_>=(expanded_?314-view_left_:234)||
               py_<(expanded_?6-view_top_:6)||py_>=(expanded_?174-view_top_:154);
    }
    // Native camera bounds include space already visible in the expanded
    // margins. Do not pan into that invisible 40/10-pixel reserve: reversing
    // there otherwise consumes many ticks before the world moves again.
    static void pan_bounds(bool expanded,int maximum_x,int maximum_y,int& left,int& right,int& top,int& bottom){
        const int mx=expanded?std::min(40,maximum_x/2):0;
        const int my=expanded?std::min(10,(maximum_y-32)/2):0;
        left=mx;right=maximum_x-mx;top=32+my;bottom=maximum_y-my;
    }
    bool focusing() const {return focus_unit_!=0;}
    void focus_selection(uint32_t unit,uint64_t frame) {
        focus_pointer_valid_=pointer_valid_;focus_pointer_x_=px_;focus_pointer_y_=py_;
        release();
        player_=read(0x03003536,1);cursor_=player_<2?read(0x03003a90+16*player_):0;
        if(!eligible() || unit<0x02000000 || unit>0x0203fec0 || (unit&3))return;
        focus_action_=read(cursor_+77,1);
        focus_selected_=focus_action_==18||focus_action_==20;
        if(focus_selected_){if(read(cursor_+96)!=unit)return;}
        else if(focus_action_!=17&&focus_action_!=19)return;
        focus_unit_=unit;focus_start_=focus_stable_=frame;
        focus_x_=int32_t(read(0x03001bb0+8*player_));
        focus_y_=int32_t(read(0x03001bb4+8*player_));
    }
    void pointer(int x,int y) {
        if(!iw_)return;
        if(focusing()){
            // Keep centering through small pointer jitter; intentional movement
            // hands control back without waiting for camera settle/timeout.
            if(!focus_pointer_valid_||(std::abs(x-focus_pointer_x_)<=12&&std::abs(y-focus_pointer_y_)<=12))return;
            release();
        }
        if(x<(expanded_?-view_left_:0)||y<(expanded_?-view_top_:0)||x>=(expanded_?320-view_left_:240)||y>=(expanded_?180-view_top_:160)){pointer_valid_=false;return;}
        pointer_valid_=true;
        px_=x;py_=y;
        if(read(0x03004840)==1)wanted_=true;
    }
    void tick(uint64_t frame) {
        if(focusing()) {
            const auto action=read(cursor_+77,1);
            if(frame<focus_start_ || frame-focus_start_>120 || !eligible() ||
               (focus_selected_ ? (action!=focus_action_||read(cursor_+96)!=focus_unit_) :
                ((action!=focus_action_ && action!=focus_action_+1) ||
                 (action==focus_action_ && frame-focus_start_>=24) ||
                 (action==focus_action_+1 && read(cursor_+96)!=focus_unit_)))) {release();return;}
            const int x=int32_t(read(0x03001bb0+8*player_));
            const int y=int32_t(read(0x03001bb4+8*player_));
            if(x!=focus_x_ || y!=focus_y_ || (!focus_selected_&&action==focus_action_))focus_stable_=frame;
            focus_x_=x;focus_y_=y;
            // Native following performs all map streaming and boundary clamps.
            // Leave ownership released until a fresh pointer event resumes it.
            if(frame-focus_stable_>=6)release();
            return;
        }
        if(frame<last_frame_){release();last_frame_=frame;return;}
        const unsigned dt=unsigned(std::min<uint64_t>(frame-last_frame_,2));last_frame_=frame;
        if(!wanted_)return;
        if(!locked_){player_=read(0x03003536,1);cursor_=player_<2?read(0x03003a90+16*player_):0;}
        if(!eligible()){release();return;}
        if(!locked_){
            cx_=int32_t(read(0x03001bb0+8*player_));cy_=int32_t(read(0x03001bb4+8*player_));
            if(cx_<0||cy_<0||cx_>8192||cy_>8192){release();return;}
            locked_=true;
        }
        const int max_x=int(read(0x0300700c,2))*8-241;
        const int max_y=int(read(0x03007004,2))*8-225;
        if(max_x<0||max_x>8192||max_y<32||max_y>8192){release();return;}
        // Edge panning is independent of cursor travel and movement range.
        // Window-leave events retain the corresponding clamped edge position.
        if(pointer_valid_&&px_<(expanded_?6-view_left_:6))cx_-=speed_*dt;
        else if(pointer_valid_&&px_>=(expanded_?314-view_left_:234))cx_+=speed_*dt;
        if(pointer_valid_&&py_<(expanded_?6-view_top_:6))cy_-=speed_*dt;
        else if(pointer_valid_&&py_>=(expanded_?174-view_top_:154))cy_+=speed_*dt;
        int left,right,top,bottom;pan_bounds(expanded_,max_x,max_y,left,right,top,bottom);
        cx_=std::clamp(cx_,left,right);cy_=std::clamp(cy_,top,bottom);
    }
};
} // namespace gbarecomp
