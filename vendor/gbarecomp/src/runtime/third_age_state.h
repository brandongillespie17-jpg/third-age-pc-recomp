#pragma once
// Native selection cursor shapes: 24 for a large unit (Mumakil), 25 for
// one tile. Mask 0x7e accepts both, ignoring the native high flag bit.
// These are visual shapes, not different input/cursor object types.
// Read-only, exact-ROM adapter. No bus accessors: reads cannot tick devices or
// trigger IO side effects. Capture on the game thread at a presentation boundary.
#include <cstdint>
#include <cstddef>
namespace gbarecomp {
struct ThirdAgeState {
    // Availability is explicit; zero alone does not mean a verified field.
    enum : uint32_t { Scene=1, Menu=2, Battle=4, Camera=8, Cursor=16, Unit=32, AI=64 };
    uint32_t valid=0, scene=0, scene_arg=0, selection=0, substate=0, next_scene=0;
    int player=-1, side=-1, turn_mode=-1, setup_phase=-1, ai_active=-1, ai_enabled=-1;
    // Control: -1 unknown, 0 transition, 1 human input gate, 2 AI gate.
    int control=-1, action=-1;
    int32_t camera_x=0,camera_y=0,cursor_x_fp=0,cursor_y_fp=0;
    uint32_t cursor_address=0,selected_address=0;
};
class ThirdAgeStateReader {
    const uint8_t* iw_=nullptr; const uint8_t* ew_=nullptr;
    uint32_t read(uint32_t a,unsigned n=4)const {
        const uint8_t* p=nullptr;
        if(iw_ && a>=0x03000000 && uint64_t(a)+n<=0x03008000)p=iw_+a-0x03000000;
        else if(ew_ && a>=0x02000000 && uint64_t(a)+n<=0x02040000)p=ew_+a-0x02000000;
        if(!p)return 0;
        uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(p[i])<<(8*i);return v;
    }
    static bool object(uint32_t a,unsigned size) {
        return a>=0x02000000 && !(a&3) && uint64_t(a)+size<=0x02040000;
    }
public:
    ThirdAgeStateReader(const uint8_t* iw,const uint8_t* ew):iw_(iw),ew_(ew){}
    ThirdAgeState capture()const {
        ThirdAgeState s;
        if(!iw_||!ew_)return s;
        s.scene=read(0x03004840);s.scene_arg=read(0x03004844);
        s.selection=read(0x03004848);s.substate=read(0x03004850);s.next_scene=read(0x03004880);
        // Known scene range; raw values remain available outside it.
        if(s.scene>=1 && s.scene<=25)s.valid|=ThirdAgeState::Scene;
        if(s.scene==5 && s.next_scene==5 && s.selection<=3)s.valid|=ThirdAgeState::Menu;
        // Dialog/combat scenes may retain stale battlefield pointers. Do not
        // pretend those are a currently interactive battlefield snapshot.
        if(s.scene!=1 || s.next_scene!=1)return s;
        const unsigned player=read(0x03003536,1);
        const uint32_t world=read(0x03004898);
        if(player>=2 || !object(world,0x300))return s;
        s.valid|=ThirdAgeState::Battle;s.player=int(player);
        s.side=int(read(0x03003533,1));s.turn_mode=int(read(0x03003534,1));
        s.setup_phase=int(read(world+0x177+0xdc*player,1));
        s.camera_x=int32_t(read(0x03001bb0+8*player));
        s.camera_y=int32_t(read(0x03001bb4+8*player));
        if(s.camera_x>=-8192&&s.camera_x<=8192&&s.camera_y>=-8192&&s.camera_y<=8192)s.valid|=ThirdAgeState::Camera;
        const uint32_t c=read(0x03003a90+16*player);
        if(object(c,0x140) && (read(c+12,1)&0x7e)==24) {
            s.valid|=ThirdAgeState::Cursor;s.cursor_address=c;
            s.cursor_x_fp=int32_t(read(c+24));s.cursor_y_fp=int32_t(read(c+28));
            s.action=int(read(c+77,1));
            const uint32_t u=read(c+96);
            if(object(u,0x140) && (read(u+12)&0x10)) {
                s.valid|=ThirdAgeState::Unit;s.selected_address=u;
            }
        }
        const uint32_t ai=read(0x03007434);
        if(object(ai,0x580)) {
            s.valid|=ThirdAgeState::AI;
            s.ai_active=int(read(ai+0x574,1));s.ai_enabled=int(read(ai+0x576,1));
            const bool ai_gate=s.ai_active!=0 || (s.ai_enabled!=0 && read(ai+0x579,1)==read(0x0300352e + player,1));
            if(ai_gate)s.control=2;
            else if(s.setup_phase>=1&&s.setup_phase<=4)s.control=0;
            else if(s.setup_phase==5 && s.action==30)s.control=1; // verified placement gate
            else if(s.turn_mode<=2 && s.side<=2)
                s.control=(s.turn_mode==1||(s.turn_mode==2&&s.side!=2)||(s.turn_mode==0&&s.side==2))?0:1;
        }
        return s;
    }
};
} // namespace gbarecomp
