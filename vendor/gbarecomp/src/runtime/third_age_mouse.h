#pragma once
// Native selection cursor shapes: 24 for a large unit (Mumakil), 25 for
// one tile. Mask 0x7e accepts both, ignoring the native high flag bit.
// These are visual shapes, not different input/cursor object types.
// Third Age (ATEE, SHA1 0a7a704d...) mouse adapter. Read-only game inspection;
// Actions use KEYINPUT pulses, with a guarded native destination-read shortcut
// for confirmed expanded-view moves. No direct guest memory or save writes.
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include "runtime_arm.h"

namespace gbarecomp {
class ThirdAgeMouse {
    inline static ThirdAgeMouse* active_=nullptr;
    RuntimeBusReadOverride previous_read_=nullptr;
    bool native_destination_=false;
    bool display_camera_=false;double display_x_=0,display_y_=0;
    bool expanded_=false;int view_left_=0,view_top_=0;
    bool in_view(int x,int y)const{return expanded_&&iw_&&read(0x03004840)==1?rect(x,y,-view_left_,-view_top_,320-view_left_,180-view_top_):rect(x,y,0,0,240,160);}
    const uint8_t* iw_ = nullptr;
    const uint8_t* ew_ = nullptr;
    enum Mode { None, Direct, Battle, Menu, Language, Act, Page, Cycle, DialogList, GlossaryPage, HeroSelect, HeroOpen } mode_ = None;
    uint32_t scene_ = 0;
    unsigned battle_action_ = 0, cycle_action_=0;
    uint32_t cycle_cursor_=0,cycle_unit_=0;
    int cycle_heading_=0,cycle_direction_=0,cycle_step_=1,cycle_tries_=0;
    int cycle_x_=0,cycle_y_=0;
    int unit_hover_slot_=-1,unit_hover_target_=-1;
    uint32_t unit_hover_world_=0;

    // Preserve a second click on the same unit while the first click is still
    // travelling/selecting. Never replay it into an action or item menu.
    uint32_t hero_click_unit_=0,hero_click_cursor_=0;
    unsigned hero_click_action_=0;
    int hero_click_x_=0,hero_click_y_=0;
    uint64_t hero_click_frame_=0;
    bool hero_click_queued_=false,hero_click_opening_=false;
    bool selection_centering_=false;
    uint32_t hero_open_unit_=0,hero_open_cursor_=0;
    uint32_t selection_focus_=0;
    uint64_t battle_progress_frame_=0;
    int battle_progress_x_=0,battle_progress_y_=0;
    unsigned battle_progress_action_=0;

    bool direct_move_target() const {
        // Only committed, reachable movement clicks use this shortcut. Hover,
        // hero menus, attacks, deployment and keyboard navigation keep their
        // original input path. The native destination/path checks still run.
        if(!native_destination_||!iw_||!ew_||read(0x03004840)!=1||read(0x03004880)!=1||
           !expanded_||!pending_confirm_||mode_!=Battle||
           (battle_action_!=18&&battle_action_!=20)||!human_turn())return false;
        const auto p=cursor();
        if(!object(p)||read(p+77,1)!=battle_action_)return false;
        const auto unit=read(p+96),grid=read(0x03004818);
        if(!object(unit)||!object(grid))return false;
        if(target_x_==signed_read(unit+24)/65536/32&&target_y_==signed_read(unit+28)/65536/32)return false;
        const int gx=target_x_,gy=target_y_-1;
        const unsigned n=read(grid+24,2),radius=read(0x03004920,2);
        if(!n||n>256||radius>255||gx<0||gy<0||gx>=int(n)||gy>=int(n)||
           std::abs(gx-int(read(0x03004910,1)))>int(radius)||
           std::abs(gy-int(read(0x03004911,1)))>int(radius))return false;
        const auto table=read(grid),column=read(table+4*gx);
        if(!object(table)||column<0x02000000u||uint64_t(column)+22u*gy+18>0x02040000u)return false;
        return int16_t(read(column+22*gy+16,2))<=394;
    }
    static int override_read(uint32_t pc,uint32_t address,uint32_t width,uint32_t original,uint32_t* value) {
        auto* self=active_;
        if(self&&width==2&&(pc==0x08011dda||pc==0x08011de0)&&self->direct_move_target()) {
            const auto p=self->cursor();
            if(address==p+(pc==0x08011dda?26:30)) {
                // 08011DC0 builds its candidate from these integer coordinates,
                // applies a direction, validates it (0802B2C4), then retargets
                // through 08013B8C. Supply an adjacent cell inside the map.
                // This changes neither the actor position nor the movement grid.
                *value=uint16_t(pc==0x08011dda?(self->target_x_+(self->target_x_? -1:1))*32+16:self->target_y_*32+23);
                return 1;
            }
        }
        return self&&self->previous_read_?self->previous_read_(pc,address,width,original,value):0;
    }

    bool same_hero_click(int x,int y,uint64_t frame) const {
        if(!hero_click_unit_ || frame<hero_click_frame_ || frame-hero_click_frame_>30 ||
           read(0x03004840)!=1 || !human_turn() || cursor()!=hero_click_cursor_ ||
           !object(hero_click_unit_) || !(read(hero_click_unit_+12)&0x10))return false;
        const unsigned action=read(hero_click_cursor_+77,1);
        if(action!=hero_click_action_ &&
           !(action==hero_click_action_+1 && read(hero_click_cursor_+96)==hero_click_unit_))return false;
        const unsigned player=read(0x03003536,1);
        const int ux=signed_read(hero_click_unit_+24)/65536-camera_x(player);
        const int uy=signed_read(hero_click_unit_+28)/65536-camera_y(player);
        // Camera follow may move the sprite between the two clicks. A repeat
        // at the original pointer location retains its original unit identity.
        if(!in_view(x,y) ||
           !((std::abs(x+view_left_-hero_click_x_)<=4 && std::abs(y+view_top_-hero_click_y_)<=4) ||
             (x-ux>=-11 && x-ux<=11 && y-uy>=-30 && y-uy<=3)))return false;
        return true;
    }

    bool hover_target_valid_ = false;
    int hover_x_=0,hover_y_=0,hover_tx_=0,hover_ty_=0;
    unsigned hover_action_=0;
    uint32_t hover_cursor_=0;
    uint16_t edge_=0;
    int target_x_ = 0, target_y_ = 0;
    uint64_t frame_ = 0, started_ = 0, pulse_start_ = 0, release_until_ = 0;
    uint16_t pulse_ = 0;
    bool pending_confirm_ = false;
    bool menu_pulse_ = false;
    uint32_t menu_selection_ = 0x03004848;
    uint16_t menu_activate_ = 1;
    uint32_t menu_context_=0, dialog_list_context_=0, question_context_=0;
    uint32_t opening_hover_context_=0;
    int opening_hover_x_=0,opening_hover_y_=0;
    uint64_t opening_hover_frame_=0;
    // Shared native Yes/No dialogs, regardless of their calling screen.
    bool confirmation_question() const {
        const uint32_t w=read(0x03004898);
        return read(0x03004840)==2 && read(0x03004880)==2 &&
            read(0x03004850)==4 && object(w) && w<=0x0203fc00 &&
            read(w+0x33b,1)==2 && read(w+0x336,1)==3 &&
            (read(w+0x33a,1)&3)==3 &&
            ((read(w+0x34a,2)==0xdb && read(w+0x34c,2)==0xda)||
             (read(w+0x34a,2)==0xda && read(w+0x34c,2)==0xdb)) &&
            (read(0x03004848)==1||read(0x03004848)==2);
    }
    uint32_t question_context() const {
        const uint32_t w=read(0x03004898);
        uint32_t h=w^read(0x03004860);
        for(unsigned i=0;i<3;++i)h=h*33u+read(w+0x348+2*i,2);
        for(unsigned i=0;i<4;++i)h=h*33u+read(w+0x336+i,1);
        for(unsigned i=0;i<4;++i)h=h*33u+read(w+0x2e0+4*i);
        return h?h:1u;
    }
    bool dialog_list_data() const {
        const uint32_t w=read(0x03004898);
        if(read(0x03004840)!=2||read(0x03004880)!=2||
           !object(w)||w>0x0203fc00)return false;
        const unsigned count=read(w+0x336,1);
        // Shared vertical-choice renderer and Up/Down handler, including
        // ability/item menus. Text-only messages use other dialog types.
        return count>=1&&count<=11&&read(w+0x33b,1)==0&&
            !(read(w+0x33a,1)&2);
    }
    bool dialog_list() const {return read(0x03004850)==4&&dialog_list_data();}
    uint32_t dialog_list_context() const {
        const uint32_t p=cursor(),w=read(0x03004898);
        uint32_t h=w^read(0x03004860)^read(w+0x33a,1);
        if(object(p))h^=p^read(p+96)^read(p+77,1);
        for(unsigned i=0;i<4;++i)h=h*33u+read(w+0x336+i,1);
        h=h*33u+read(w+0x2d0,1);
        for(unsigned i=0;i<read(w+0x336,1);++i)h=h*33u+read(w+0x348+2*i,2);
        return h?h:1u;
    }
    bool act_unlocked(uint32_t world,unsigned act) const {
        // Mirror the game's act availability check (08022bb0). Selection still
        // goes through ordinary L/R input; this never changes progression.
        if(!object(world)||world>0x0203fb00||act>2)return false;
        if(act==0)return true;
        if(read(0x030040ec,1)==0) {
            const unsigned side=read(world+0x2cb,1);
            if(side>1)return false;
            return (read(0x030048f0+8*side)&(act==1?0x8u:0x1000u))!=0;
        }
        const uint32_t flags=(read(0x030047f8)&0x20)?0x03007124:world+0x474;
        return (read(flags,2)&(act==1?0x400u:0x800u))!=0;
    }
    // Menus share this hit test for both hover and click. Empty areas are not
    // confirm buttons. Gameplay targeting is deliberately separate.
    bool menu_hit(int x,int y,uint32_t scene,int& tx,int& ty,Mode& mode,
                  uint32_t& selection,uint16_t& activate) const {
        mode=Menu;ty=0;selection=0x03004848;activate=1;
        if(!rect(x,y,0,0,240,160))return false;
        if(scene==2 && dialog_list()) {
            const uint32_t w=read(0x03004898);
            const int left=8*read(w+0x337,1)+4,top=8*read(w+0x338,1)+8;
            const int width=8*read(w+0x2d0,1)-8,count=read(w+0x336,1);
            // Native list renderer 08028d84 uses font 2: ten pixels per row.
            if(width<=0||left<0||left+width>240||top<0||top+10*count>160||
               !rect(x,y,left,top,left+width,top+10*count))return false;
            mode=DialogList;tx=(y-top)/10;
            return !(tx==0 && count>1 && (read(w+0x33a,1)&1));
        }
        if(scene==2 && confirmation_question()) {
            const uint32_t w=read(0x03004898);
            // The native renderer positions options relative to the dialog;
            // prompt width and language can move them (08028edc).
            const int left=8*read(w+0x337,1),top=8*read(w+0x338,1);
            const int x1=left+8+signed_read(w+0x2e0),x2=left+8+signed_read(w+0x2e8);
            const int y1=top+8+signed_read(w+0x2e4),y2=top+8+signed_read(w+0x2ec);
            const int low=std::min(x1,x2),high=std::max(x1,x2);
            const int right=left+8*read(w+0x2d0,1);
            if(low<left||high==low||high>=right||right>240||y1!=y2||
               y1<0||y1+14>160)return false;
            const int split=(low+high)/2+10;
            if(rect(x,y,low-8,y1-3,split,y1+14))tx=x1<x2?1:2;
            else if(rect(x,y,split,y1-3,std::min(right-4,high+28),y1+14))tx=x1<x2?2:1;
            else return false;
            return true;
        }
        if(scene==19) { // Glossary: native pages, no selectable text entries.
            if(rect(x,y,0,0,28,28)||rect(x,y,212,0,240,28)) {
                const unsigned current=read(0x03004848);
                if(read(0x03004844)!=1||current>5)return false;
                mode=GlossaryPage;tx=(current+(x<28?5:1))%6;
                activate=x<28?512:256;return true;
            }
            return false;
        }
        if(scene==22) { // Units: the three visible carousel figures.
            const uint32_t w=read(0x03004898);
            if(!object(w)||w>0x0203fc00||read(0x03004880)!=22||
               !rect(x,y,44,122,195,160))return false;
            const unsigned count=read(w+0x3d4,1),current=read(0x03004848);
            if(count<1||count>14||current>=count)return false;
            const int slot=x<95?0:x>=145?2:1;
            // Hover rotates the carousel. Keep the selected identity until
            // the pointer leaves this slot, so its subsequent click agrees.
            if(unit_hover_world_==w&&slot==unit_hover_slot_&&int(current)==unit_hover_target_)tx=current;
            else tx=slot==0?(current+count-1)%count:slot==2?(current+1)%count:current;
            activate=0;return true; // selecting already displays this unit
        }
        if(scene==5) {
            if(!rect(x,y,58,68,186,134))return false;
            tx=std::min(3,(y-68)/16);return true;
        }
        if(scene==6 && (read(0x03004848)==5||read(0x03004848)==15)) {
            // Main-menu Options has nine visible rows (internal index 6 is
            // hidden here). Hover selects; clicking a setting changes it.
            if(!rect(x,y,35,2,205,137))return false;
            static const int rows[]={0,1,2,3,4,5,7,8,9};
            tx=rows[std::min(8,(y-2)/15)];selection=0x0300484c;
            if(read(0x03004848)==15&&tx==0)return false; // fixed during battle
            if(tx<=5)activate=(x>=120&&x<152)?32:16;
            return true;
        }
        if(scene==15) {
            if(read(0x03004850)==1) {
                if(!rect(x,y,60,146,162,160))return false;
                tx=x<112?0:1;selection=0x0300484c;return true;
            }
            if(read(0x03004850)!=0||!rect(x,y,34,133,205,160))return false;
            tx=(x<120?0:2)+(y>=146?1:0);return true;
        }
        if(scene==7) {
            selection=0x0300484c;
            if(rect(x,y,18,25,114,103)){tx=0;return true;}
            if(rect(x,y,126,25,222,103)){tx=1;return true;}
            return false;
        }
        if(scene==25) {
            if(rect(x,y,76,98,164,152)){tx=2;return true;}
            if(rect(x,y,18,26,106,98)){tx=1;return true;}
            if(rect(x,y,132,26,220,98)){tx=0;return true;}
            return false;
        }
        if(scene==23) {
            // Mission IDs are not row numbers (Rivendell is ID 0). Use live
            // visible text objects and the list ordinal, including unlocked rows.
            const uint32_t world=read(0x03004898);
            if(!object(world)||world>0x0203fc00||read(world+0x29c,1)!=1)return false;
            if(rect(x,y,12,1,65,18)) {
                tx=x<29?0:x<47?1:2;
                const unsigned current=read(world+0x29e,1);
                if(current>2)return false;
                for(unsigned act=std::min(unsigned(tx),current);act<=std::max(unsigned(tx),current);++act)
                    if(!act_unlocked(world,act))return false;
                mode=Act;selection=world+0x29e;activate=0;return true;
            }
            if(!rect(x,y,2,28,74,145))return false;
            const unsigned count=read(world+0x2a0,1);
            if(count==0||count>14)return false;
            for(unsigned row=0;row<count;++row) {
                const uint32_t item=read(world+0x258+4*row);
                if(!object(item)||!(read(item+12)&1))continue;
                const int top=signed_read(item+28)/65536-9;
                if(top<20||top>=145)continue;
                int bottom=std::min(145,top+33);
                for(unsigned next=row+1;next<count;++next) {
                    const uint32_t other=read(world+0x258+4*next);
                    if(object(other)&&(read(other+12)&1)) {
                        const int next_top=signed_read(other+28)/65536-9;
                        if(next_top>top){bottom=std::min(bottom,next_top);break;}
                    }
                }
                if(y>=top&&y<bottom){tx=int(row);selection=world+0x29f;return true;}
            }
            return false;
        }
        if(scene==28) {
            const uint32_t world=read(0x03004898);
            if(!object(world)||!rect(x,y,32,30,212,113))return false;
            const unsigned count=read(world+1,1);
            tx=(x-32)/44;return count>=1&&count<=4&&unsigned(tx)<count;
        }
        if(scene==26||scene==27||scene==30) {
            // Native L/R page buttons. Hovering a tab never purchases anything.
            if(rect(x,y,0,0,26,28)||rect(x,y,214,0,240,28)) {
                mode=Page;tx=0;activate=x<26?512:256;return true;
            }
            selection=0x0300484c;
            if(scene==26) {
                if(!rect(x,y,34,31,210,119))return false;
                // Native ability renderer 08034ec8: commanders have four
                // cards; companions have one centered card (08034f24).
                // 03001bea selects the layout, not whether upgrades work.
                if(!read(0x03001bea,1)){tx=0;return true;}
                if((x>=114&&x<126)||(y>=74&&y<78))return false;
                tx=(x>=120?1:0)+(y>=76?2:0);return true;
            }
            if(!rect(x,y,17,30,223,123))return false;
            tx=std::min(2,(x-17)/69);return true;
        }
        if(scene==18) {
            if(y<137)return false;
            tx=std::min(2,x/80);return true;
        }
        if(scene==17) {
            const uint32_t world=read(0x03004898);
            if(!object(world)||read(0x03004850)==2||read(0x03004880)!=17)return false;
            const unsigned count=read(world+0x42,1);
            if(count<1||count>8||!rect(x,y,40,32,200,112))return false;
            tx=(x-40)/40+4*((y-32)/40);
            return unsigned(tx)<count; // the eighth companion slot may be locked
        }
        if(scene==16) {
            const unsigned stage=read(0x03004850);
            if(stage==11) { // Sauron Mode? Yes / No
                if(!rect(x,y,65,138,176,151))return false;
                tx=x<120?0:1;selection=0x0300484c;return true;
            }
            if(read(0x0300484c)!=0)return false;
            if(!rect(x,y,16,27,224,113) || (x>=112&&x<128))return false;
            tx=x<112?(x-16)/32:3+(x-128)/32;return true;
        }
        if(scene==13) {
            if(rect(x,y,12,26,226,82)){tx=std::min(2,(x-12)/72);ty=0;}
            else if(rect(x,y,48,82,194,135)){tx=x<120?0:1;ty=1;}
            else return false;
            mode=Language;return true;
        }
        return false;
    }
    bool mapped_menu(uint32_t scene) const {
        return (scene==2&&(confirmation_question()||dialog_list()))||scene==15||scene==5||scene==7||scene==25||scene==23||scene==18||scene==13||scene==17||scene==28||scene==26||scene==27||scene==30||(scene==19&&read(0x03004844)==1)||scene==22||
            (scene==16&&(read(0x0300484c)==0||read(0x03004850)==11))||(scene==6&&(read(0x03004848)==5||read(0x03004848)==15));
    }
    void start_menu(int x,int y,uint64_t frame,bool confirm) {
        int tx=0,ty=0;Mode mode=None;uint32_t selection=0;uint16_t activate=1;
        uint32_t scene=read(0x03004840);
        if(!menu_hit(x,y,scene,tx,ty,mode,selection,activate)) {
            if(!confirm && !pending_confirm_ && (mode_==Menu||mode_==Language||mode_==Act||mode_==DialogList))cancel();
            return;
        }
        if((mode==Page||mode==GlossaryPage)&&!confirm)return;
        const uint32_t context=mode==DialogList?dialog_list_context():scene==23?read(read(0x03004898)+0x29e,1):read(0x03004850);
        if(!confirm && mode_==mode && scene_==scene && target_x_==tx && target_y_==ty &&
           (mode!=Menu||scene!=23||menu_context_==context)&&
           (mode!=DialogList||dialog_list_context_==context))return;
        const bool retarget=!confirm && mode_==mode && scene_==scene &&
            menu_selection_==selection && ((scene!=15&&scene!=16&&scene!=23)||menu_context_==context)&&
            (mode!=DialogList||dialog_list_context_==context);
        if(!retarget)cancel();
        frame_=frame;started_=frame;scene_=scene;
        target_x_=tx;target_y_=ty;mode_=mode;menu_selection_=selection;
        pending_confirm_=confirm;menu_pulse_=true;menu_activate_=activate;
        menu_context_=context;
        if(scene==22 && !confirm) {
            unit_hover_slot_=x<95?0:x>=145?2:1;unit_hover_target_=tx;
            unit_hover_world_=read(0x03004898);
        }
        if(mode==DialogList)dialog_list_context_=context;
        if(scene==2 && mode==Menu)question_context_=question_context();
    }

    bool deployment_context() const {
        const unsigned player=read(0x03003536,1);
        const uint32_t p=cursor(),world=read(0x03004898);
        return player<2 && object(p) && object(world) &&
            (read(p+12,1)&0x7e)==24 && read(p+77,1)==30 &&
            read(world+0x177+0xdc*player,1)==5;
    }
    bool placing_hero() const {
        return read(0x03004840)==1 && deployment_context();
    }
    bool placement_tip() const {
        return read(0x03004840)==2 && read(0x03004860)==1 && deployment_context();
    }
    bool attack_targets() const {
        const uint32_t p=cursor();
        return read(0x03004840)==1&&object(p)&&(read(p+12,1)&0x7e)==24&&
            (read(p+77,1)==25||read(p+77,1)==22);
    }
    bool human_turn() const {
        const auto ai=read(0x03007434),player=read(0x03003536,1);
        if(ai<0x02000000||ai>0x0203fa00||player>=2)return false;
        if(read(ai+0x574,1)!=0 || (read(ai+0x576,1)!=0 &&
            read(ai+0x579,1)==read(0x0300352e + player,1)))return false;
        if(placing_hero())return true; // deployment precedes the normal turn gate
        const auto turn=read(0x03003534,1),side=read(0x03003533,1);
        return !(turn==1 || (turn==2&&side!=2) || (turn==0&&side==2));
    }
    void start_battle(int x,int y,uint64_t frame,bool confirm) {
            if(!human_turn()){cancel();return;}
            if(!in_view(x,y)){if(!pending_confirm_)cancel();return;}
            uint32_t p=cursor(); if(!object(p) || (read(p+12,1)&0x7e)!=24) {cancel();return;}
            const unsigned player=read(0x03003536,1);
            int camera_x=this->camera_x(player),camera_y=this->camera_y(player);
            if(camera_x<0 || camera_y<0 || camera_x>8192 || camera_y>8192) {cancel();return;}
            const int wx=x+camera_x,wy=y+camera_y;
            if(wx<0||wy<0||wx>=int(read(0x0300700c,2))*8||wy>=int(read(0x03007004,2))*8){cancel();return;}
            if(expanded_&&(wx>=int(read(0x0300700c,2))*8-1||wy<32||wy>=int(read(0x03007004,2))*8-65)){cancel();return;}
            int tx=wx/32,ty=wy/32;
            // Unit origin is at (tile centre X, tile Y+24). Prefer a visible
            // body under the pointer; don't accidentally target the tile
            // above a tall sprite. Scan the game's live linked object list.
            int best=100000;uint32_t hit_unit=0;
            uint32_t u=read(0x03002e40);
            // Actions 18 (normal) and 20 (tutorial) preview movement: use the actual ground tile.
            for(int i=0;i<512&&object(u)&&read(p+77,1)!=20&&read(p+77,1)!=18&&read(p+77,1)!=30;++i) {
                unsigned kind=read(u+8,1);
                if(kind==5 || kind==6) {
                    int ux=signed_read(u+24)/65536,uy=signed_read(u+28)/65536;
                    const int dx=wx-ux,dy=wy-uy;
                    // Only live renderable unit objects are hit targets.
                    if((read(u+12)&0x10) && dx>=-11&&dx<=11&&dy>=-30&&dy<=3) {
                        int score=dx*dx+(dy+12)*(dy+12);
                        if(score<best){best=score;tx=ux/32;ty=uy/32;hit_unit=u;}
                    }
                }
                uint32_t next=read(u);if(next==u)break;u=next;
            }
            // During movement, keep ground-tile targeting elsewhere, but
            // let the selected unit's body select its own tile. A second A on
            // a hero opens the native ability/item menu instead of moving to
            // the tile behind the upper part of its sprite.
            if(read(p+77,1)==18||read(p+77,1)==20) {
                const uint32_t selected=read(p+96);
                if(object(selected)&&(read(selected+12)&0x10)&&
                   (read(selected+8,1)==5||read(selected+8,1)==6)) {
                    const int ux=signed_read(selected+24)/65536,uy=signed_read(selected+28)/65536;
                    if(wx-ux>=-11&&wx-ux<=11&&wy-uy>=-30&&wy-uy<=3){tx=ux/32;ty=uy/32;hit_unit=selected;}
                }
            }
            if(confirm && hover_target_valid_ && x==hover_x_ && y==hover_y_ &&
               p==hover_cursor_ && read(p+77,1)==hover_action_) {
                tx=hover_tx_;ty=hover_ty_;
            }
            // Retargeting during motion preserves the key release interval.
            if(mode_!=Battle || confirm)cancel();
            mode_=Battle;scene_=1;frame_=frame;started_=frame;
            menu_pulse_=false;menu_activate_=1;
            target_x_=tx;target_y_=ty;pending_confirm_=confirm;
            battle_action_=read(p+77,1);
            battle_progress_frame_=frame;battle_progress_x_=signed_read(p+24);battle_progress_y_=signed_read(p+28);battle_progress_action_=battle_action_;
            if(confirm && hit_unit && (battle_action_>=17&&battle_action_<=20) &&
               tx==signed_read(hit_unit+24)/65536/32 && ty==signed_read(hit_unit+28)/65536/32) {
                hero_click_unit_=hit_unit;hero_click_cursor_=p;
                hero_click_opening_=battle_action_==18||battle_action_==20;
                hero_click_action_=hero_click_opening_?battle_action_-1:battle_action_;
                hero_click_x_=x+view_left_;hero_click_y_=y+view_top_;hero_click_frame_=frame;
            }
            edge_=(confirm||placing_hero())?0:x<(expanded_?6-view_left_:6)?32:x>=(expanded_?314-view_left_:234)?16:y<(expanded_?6-view_top_:6)?64:y>=(expanded_?174-view_top_:154)?128:0;
            if(!confirm) {
                hover_target_valid_=true;hover_x_=x;hover_y_=y;
                hover_tx_=tx;hover_ty_=ty;hover_action_=battle_action_;hover_cursor_=p;
            }
    }


    uint32_t read(uint32_t a, unsigned n=4) const {
        const uint8_t* p = nullptr;
        if (a >= 0x03000000 && a + n <= 0x03008000) p=iw_+(a-0x03000000);
        if (a >= 0x02000000 && a + n <= 0x02040000) p=ew_+(a-0x02000000);
        if (!p) return 0;
        uint32_t v=0; for(unsigned i=0;i<n;++i)v|=uint32_t(p[i])<<(8*i); return v;
    }
    static bool object(uint32_t p) { return p>=0x02000000 && p<=0x0203fec0 && !(p&3); }
    int signed_read(uint32_t a) const { return static_cast<int32_t>(read(a)); }
    uint32_t cursor() const {
        const unsigned player=read(0x03003536,1);
        return player<2 ? read(0x03003a90+16*player) : 0;
    }
    void pulse(uint16_t k) { pulse_=k; pulse_start_=std::max(frame_,release_until_); }
    static bool rect(int x,int y,int l,int t,int r,int b) { return x>=l&&x<r&&y>=t&&y<b; }
public:
    void set_expanded(bool value,int left=40,int top=10){if(expanded_!=value){cancel();expanded_=value;}view_left_=value?left:0;view_top_=value?top:0;}
    void bind(const uint8_t* iw,const uint8_t* ew,bool native_destination=false) {
        iw_=iw;ew_=ew;native_destination_=native_destination;
        if(active_!=this){previous_read_=g_runtime_bus_read_override;active_=this;g_runtime_bus_read_override=&override_read;}
    }
    ~ThirdAgeMouse(){
        if(active_==this){if(g_runtime_bus_read_override==&override_read)g_runtime_bus_read_override=previous_read_;active_=nullptr;}
    }
    void cancel() { opening_hover_context_=0;selection_focus_=0;hero_click_unit_=0;hero_click_queued_=hero_click_opening_=false;mode_=None; pulse_=0; pending_confirm_=false;hover_target_valid_=false;edge_=0;cycle_action_=0;dialog_list_context_=0;question_context_=0; }
    uint32_t take_selection_focus() { const auto unit=selection_focus_;selection_focus_=0;return unit; }
    void set_display_camera(bool valid,double x,double y){
        if(valid!=display_camera_||(valid&&(x!=display_x_||y!=display_y_)))hover_target_valid_=false;
        display_camera_=valid;display_x_=x;display_y_=y;
    }
    int camera_x(unsigned player)const{return display_camera_?int(std::floor(display_x_)):signed_read(0x03001bb0+8*player);}
    int camera_y(unsigned player)const{return display_camera_?int(std::floor(display_y_)):signed_read(0x03001bb4+8*player);}
    void set_selection_centering(bool active){selection_centering_=active;}
    bool repeats_selection(int x,int y,uint64_t frame)const{return same_hero_click(x,y,frame);}
    bool busy() const { return mode_!=None || pulse_!=0 || hero_click_queued_; }
    // Pointer motion changes menu highlight or battle cursor. A committed click
    // keeps its own target even if the pointer moves before activation.
    void hover(int x,int y,uint64_t frame) {
        if(expanded_&&iw_&&read(0x03004840)==2&&read(0x03004860)==1){x+=view_left_-40;y+=view_top_-10;}
        if(!iw_||!ew_)return;
        // The HD list is already visible during its native opening fade, but
        // Up/Down input is not accepted until stage 4. Keep the latest pointer
        // target through that handoff; never turn a hover into confirmation.
        if(read(0x03004850)<=2&&dialog_list_data()){
            cancel();opening_hover_context_=dialog_list_context();
            opening_hover_x_=x;opening_hover_y_=y;opening_hover_frame_=frame;return;
        }
        opening_hover_context_=0;
        if(pending_confirm_||mode_==Direct||mode_==HeroSelect||mode_==HeroOpen)return;
        if(same_hero_click(x,y,frame))return; // keep the clicked unit through camera follow
        if(read(0x03004840)!=22||!rect(x,y,44,122,195,160))unit_hover_slot_=-1;
        if(placing_hero()||attack_targets()) {
            if(mode_==Battle)cancel();
            return; // wheel owns selection; pointer position has no target
        }
        if(read(0x03004840)==1){start_battle(x,y,frame,false);return;}
        start_menu(x,y,frame,false);
    }
    // A repeat click on the same unit queues its menu; other clicks retarget.
    // button: 1 left, 2 middle/Select, 3 right/B, 4 wheel up, 5 wheel down.
    void click(int x,int y,int button,uint64_t frame,int clicks=1) {
        if(expanded_&&iw_&&read(0x03004840)==2&&read(0x03004860)==1){x+=view_left_-40;y+=view_top_-10;}
        if(iw_&&ew_&&button==1&&same_hero_click(x,y,frame)){
            // Clicking an already selected hero has already requested its menu.
            // Keep that request through centering; never replay the repeat into it.
            if(!hero_click_opening_)hero_click_queued_=true;
            return;
        }
        release_until_=frame+2; // guarantee a new press, even for rapid repeated clicks
        if(iw_&&ew_&&(placing_hero()||attack_targets()||placement_tip())) {
            const bool tip=placement_tip(),attack=attack_targets();
            cancel();menu_pulse_=false;frame_=frame;started_=frame;scene_=read(0x03004840);
            if(!in_view(x,y))return;
            if(tip){if(button==1){mode_=Direct;pulse(1);}return;}
            if(!human_turn())return;
            const uint32_t p=cursor(),unit=read(p+96);
            cycle_action_=read(p+77,1);
            if(attack&&(button==4||button==5)) {
                if(cycle_cursor_!=p||cycle_unit_!=unit)cycle_heading_=0;
                cycle_cursor_=p;cycle_unit_=unit;
                cycle_step_=button==5?1:-1;
                cycle_direction_=(cycle_heading_+(button==4?2:0))%4;
                cycle_tries_=0;cycle_x_=signed_read(p+24);cycle_y_=signed_read(p+28);
                mode_=Cycle;return;
            }
            const uint16_t key=button==1?1:button==3?2:button==2?4:
                (button==4||button==5)?256:0;
            if(key){mode_=Direct;pulse(key);}return;
        }
        if(iw_&&ew_&&button==1&&read(0x03004840)==1) {
            start_battle(x,y,frame,true);
            // SDL retains the double-click count even when both button-downs
            // arrive in one host poll. Queue only a live unit's first selection;
            // never turn one menu click into two confirms.
            if(clicks>=2&&hero_click_unit_&&!hero_click_opening_)hero_click_queued_=true;
            return;
        }
        cancel(); menu_pulse_=false; menu_activate_=1; frame_=frame; started_=frame;
        if (!iw_ || !ew_ || !in_view(x,y)) return;
        scene_=read(0x03004840);
        if(scene_!=22||button!=1)unit_hover_slot_=-1;
        if(button==3 && scene_==19 && read(0x03004844)==1) {
            mode_=GlossaryPage;target_x_=-1;menu_activate_=2;
            pending_confirm_=true;menu_pulse_=true;return;
        }
        if(button!=1) {
            uint16_t k=button==3?2:button==2?4:button==4?(scene_==22?32:64):button==5?(scene_==22?16:128):0;
            if(k){mode_=Direct;pulse(k);}return;
        }
        if(mapped_menu(scene_)) {
            start_menu(x,y,frame,true);return;
        }
        if(scene_==29) { mode_=Direct;pulse(8);return; }
        // Existing unmapped screens retain confirm-current behavior; hover
        // never activates them. Mapped menus ignore clicks outside entries.
        mode_=Direct;pulse(1);
    }

    uint16_t update(uint64_t frame,uint16_t physical=0x3ff,bool blocked=false,bool edge_camera_panning=false) {
        if(!iw_ || !ew_)return physical;
        if(blocked || physical!=0x3ff || frame<frame_) {unit_hover_slot_=-1;cancel();release_until_=0;frame_=frame;return physical;}
        frame_=frame;
        if(edge_camera_panning && mode_==Battle && !pending_confirm_) {
            // The independent edge camera owns movement here. Do not chase
            // each newly exposed tile with native cursor/D-pad work as well.
            // Discard, rather than pause, the old hover so loss of camera
            // ownership cannot resurrect it. Fresh interior motion maps the
            // displayed world again; committed clicks keep their own target.
            // Do not cancel(): a recent hero click may still own its identity.
            mode_=None;pulse_=0;edge_=0;hover_target_valid_=false;
        }
        if(opening_hover_context_){
            if(!dialog_list_data()||dialog_list_context()!=opening_hover_context_||
               read(0x03004850)==3||frame<opening_hover_frame_||frame-opening_hover_frame_>120)opening_hover_context_=0;
            else if(dialog_list()){
                const int x=opening_hover_x_,y=opening_hover_y_;opening_hover_context_=0;
                start_menu(x,y,frame,false);
            }
        }
        // Keep the clicked identity alive through native centering. The camera
        // bounds its focus lifetime; ordinary repeat recognition stays 30 frames.
        if(selection_centering_&&hero_click_unit_)hero_click_frame_=frame;
        if(hero_click_unit_ && !hero_click_queued_ && !pending_confirm_ && mode_!=HeroSelect && frame-hero_click_frame_>30)hero_click_unit_=0;
        if(hover_target_valid_ && (read(0x03004840)!=1 || cursor()!=hover_cursor_ ||
           read(hover_cursor_+77,1)!=hover_action_))hover_target_valid_=false;
        if(!busy())return physical;
        if(question_context_ && (!confirmation_question()||question_context()!=question_context_)) {
            cancel();return physical;
        }
        if(dialog_list_context_ && (!dialog_list()||dialog_list_context()!=dialog_list_context_)) {
            cancel();return physical;
        }
        if(cycle_action_ && (!human_turn()||!object(cursor())||read(cursor()+77,1)!=cycle_action_)) {
            cancel();return physical;
        }
        if(mode_==Battle&&(placing_hero()||attack_targets())){cancel();return physical;}
        if((scene_==26||scene_==27||scene_==28||scene_==30||scene_==19||scene_==22)&&read(0x03004880)!=scene_) {
            cancel();return physical;
        }
        if(scene_==2 && mode_==Menu && !confirmation_question()){cancel();return physical;}
        if(scene_==17 && mode_==Menu &&
           (read(0x03004850)==2||read(0x03004880)!=17)){cancel();return physical;}
        if((scene_==15||scene_==16) && mode_==Menu && read(0x03004850)!=menu_context_){cancel();return physical;}
        if(read(0x03004840)!=scene_ || (frame-started_>240 && !(mode_==Battle&&edge_&&!pending_confirm_))) {cancel();return physical;}
        if(scene_==23 && (mode_==Menu||mode_==Act)) {
            const uint32_t world=read(0x03004898);
            if(!object(world)||world>0x0203fc00){cancel();return physical;}
            if(mode_==Menu && read(world+0x29e,1)!=menu_context_){cancel();return physical;}
            // Act changes rebuild this same scene behind a fade. Do not send
            // repeated shoulders or confirm a row while the list is rebuilding.
            if(read(0x03004880)!=23 || read(world+0x29c,1)!=1 || read(world+0x2a0,1)==0) {
                pulse_=0;release_until_=frame+2;return physical;
            }
        }
        if(mode_==HeroSelect){
            const auto p=cursor();const auto action=object(p)?read(p+77,1):0;
            if(!human_turn()||p!=hero_click_cursor_||!object(hero_click_unit_)||
               !(read(hero_click_unit_+12)&0x10)||frame-started_>=24){cancel();return physical;}
            if(action==hero_click_action_+1&&read(p+96)==hero_click_unit_){
                mode_=None;pulse_=0;release_until_=frame+2;
                // The second click still refers to this unit after selection.
                // Leave the actual repeat-click deadline tied to the click.
                if(!hero_click_queued_)return physical;
            }else if(action!=hero_click_action_){cancel();return physical;}
        }
        if(hero_click_queued_) {
            const uint32_t unit=hero_click_unit_,p=cursor();
            const unsigned action=object(p)?read(p+77,1):0;
            if(!human_turn() || p!=hero_click_cursor_ || !object(unit) ||
               !(read(unit+12)&0x10) ||
               (action!=hero_click_action_ && action!=hero_click_action_+1)) {
                cancel();return physical;
            }
            if(action==hero_click_action_+1) {
                if(read(p+96)!=unit){cancel();return physical;}
                // Do not spend the second A while the first click's camera
                // operation owns input. Keep the identity/intent, not a pulse.
                if(selection_centering_){pulse_=0;return physical;}
                const int tx=signed_read(unit+24)/65536/32,ty=signed_read(unit+28)/65536/32;
                cancel();mode_=Battle;scene_=1;started_=frame;
                battle_action_=action;target_x_=tx;target_y_=ty;
                battle_progress_frame_=frame;battle_progress_x_=signed_read(p+24);battle_progress_y_=signed_read(p+28);battle_progress_action_=action;
                pending_confirm_=true;menu_pulse_=false;menu_activate_=1;
                release_until_=frame+2;
                return physical; // distinct native A press, after selection acknowledgement
            }
        }
        // A committed click must not monopolize hover after an unreachable
        // tile or rejected selection. Moving cursors keep their full travel time.
        if(mode_==Battle&&pending_confirm_){
            const auto p=cursor();const int x=signed_read(p+24),y=signed_read(p+28);const auto action=read(p+77,1);
            if(x!=battle_progress_x_||y!=battle_progress_y_||action!=battle_progress_action_){
                battle_progress_frame_=frame;battle_progress_x_=x;battle_progress_y_=y;battle_progress_action_=action;
            }else if(frame-battle_progress_frame_>=24){cancel();return physical;}
        }
        if(mode_==HeroOpen) {
            const uint32_t p=cursor(),unit=hero_open_unit_;
            if(!human_turn() || p!=hero_open_cursor_ || !object(p) || !object(unit) ||
               (read(p+12,1)&0x7e)!=24 || !(read(unit+12)&0x10) ||
               read(p+77,1)!=battle_action_ || read(p+96)!=unit || (!selection_centering_&&frame-started_>=24) ||
               signed_read(p+24)/65536/32!=signed_read(unit+24)/65536/32 ||
               signed_read(p+28)/65536/32!=signed_read(unit+28)/65536/32) {
                cancel();return physical;
            }
            // A selected hero may be outside the original 240x160 viewport.
            // Native menu input waits for camera follow; don't time it out or
            // spend an A press while that bounded focus operation is running.
            if(selection_centering_){started_=frame;pulse_=0;return physical;}
        }
        if(frame<release_until_)return 0x3ff;
        if(mode_==Cycle) {
            static const uint16_t keys[]={16,128,32,64}; // right, down, left, up
            if(pulse_) {
                if(frame-pulse_start_<2)return uint16_t(0x3ff&~pulse_);
                if(frame-pulse_start_<4)return 0x3ff;
                pulse_=0;
                const uint32_t p=cursor();
                if(signed_read(p+24)!=cycle_x_||signed_read(p+28)!=cycle_y_) {
                    cycle_heading_=(cycle_direction_+(cycle_step_<0?2:0))%4;
                    cancel();return physical;
                }
                // Native target navigation is spatial. If this direction has
                // no legal target, try the next direction, at most four total.
                if(++cycle_tries_==4){cancel();return physical;}
                cycle_direction_=(cycle_direction_+cycle_step_+4)%4;
            }
            pulse(keys[cycle_direction_]);return uint16_t(0x3ff&~pulse_);
        }
        // Battlefield movement uses the native press latch (08011dc0 /
        // 080139e4). Release as soon as the game processes a directional input,
        // and resume as soon as it acknowledges neutral. Recompute the latest
        // mouse direction each sample instead of finishing a stale pulse.
        if(mode_==Battle && (battle_action_==17||battle_action_==18||
                           battle_action_==19||battle_action_==20||battle_action_==31)) {
            const uint32_t p=cursor();
            if(!human_turn()||!object(p)||(read(p+12,1)&0x7e)!=24||
               read(p+77,1)!=battle_action_){cancel();return physical;}
            pulse_=0;
            if(read(p+0x8c,1)!=0)return 0x3ff;
        }
        // Navigation pulses cover one 30 Hz input tick. Timing uses guest
        // frames, never pointer-event frequency or host display refresh.
        if(pulse_) {
            const bool confirm_pulse=mode_==Direct||mode_==HeroSelect||mode_==HeroOpen;
            const uint64_t held=confirm_pulse?4:2, period=confirm_pulse?6:4;
            if(frame-pulse_start_<held)return uint16_t(0x3ff&~pulse_);
            if(frame-pulse_start_<period)return 0x3ff;
            pulse_=0;
            if(mode_==Direct){
                // Keep the first unit's identity briefly after selection. Its
                // sprite may move under a stationary pointer as the camera follows.
                if(hero_click_unit_ && cursor()==hero_click_cursor_ &&
                   read(hero_click_cursor_+77,1)==hero_click_action_+1 &&
                   read(hero_click_cursor_+96)==hero_click_unit_)mode_=None;
                else cancel();
                return 0x3ff;
            }
        }
        uint16_t direction=0;
        if(mode_==Battle) {
            if(!human_turn()){cancel();return 0x3ff;}
            uint32_t p=cursor();if(!object(p)||(read(p+12,1)&0x7e)!=24){cancel();return 0x3ff;}
            if(read(p+77,1)!=battle_action_){cancel();return 0x3ff;}
            int cx=signed_read(p+24),cy=signed_read(p+28);
            if(cx<0 || cy<0 || cx>8192*65536 || cy>8192*65536){cancel();return 0x3ff;}
            int tx=(target_x_*32+16)*65536,ty=(target_y_*32+23)*65536;
            if(!pending_confirm_&&edge_) {
                // Edge panning changes the visible map beneath a stationary
                // pointer; follow that cell without extending beyond the view.
                const unsigned player=read(0x03003536,1);
                target_x_=(hover_x_+camera_x(player))/32;
                target_y_=(hover_y_+camera_y(player))/32;
                hover_tx_=target_x_;hover_ty_=target_y_;
                tx=(target_x_*32+16)*65536;ty=(target_y_*32+23)*65536;
            }
            // The native large-unit cursor snaps any of the Mumakil's four
            // occupied cells to the bottom-right cell. Treat that whole
            // footprint as arrived; steering back to its other three cells
            // otherwise alternates forever between the unit and its neighbour.
            if((battle_action_==17||battle_action_==19)&&
               (read(p+12,1)&0x7f)==24&&
               target_x_>=cx/65536/32-1&&target_x_<=cx/65536/32&&
               target_y_>=cy/65536/32-1&&target_y_<=cy/65536/32){tx=cx;ty=cy;}
            // Cursor coordinates are 16.16. Don't confirm while it is between
            // cells; keep driving and stop without A on unreachable targets.
            if(std::abs(cx-tx)>65536)direction=cx<tx?16:32;
            // Native diagonal input avoids travelling one whole axis first.
            if(std::abs(cy-ty)>65536)direction|=cy<ty?128:64;
            if(direction&&direct_move_target())direction=target_x_?16:32;
        } else if(mode_==HeroSelect||mode_==HeroOpen) {
            if(read(0x03004230+2*read(0x03003536,1),2)&1)return physical;
            direction=1; // retry only until this same unit leaves movement preview
        } else if(mode_==Language) {
            int cx=read(0x03004848),cy=read(0x0300484c);
            if(cy!=target_y_)direction=cy<target_y_?128:64;
            else if(cx!=target_x_)direction=cx<target_x_?16:32;
        } else if(mode_==GlossaryPage) {
            const unsigned current=read(0x03004848);
            if(read(0x03004844)!=1||current>5||current==unsigned(target_x_)) {
                cancel();return physical;
            }
            direction=menu_activate_; // retry after native page redraw
        } else if(mode_==Act) {
            const uint32_t world=read(0x03004898);
            const unsigned current=read(world+0x29e,1);
            if(current>2||!act_unlocked(world,unsigned(target_x_))){cancel();return physical;}
            if(current==unsigned(target_x_)){cancel();return physical;}
            const unsigned next=current<unsigned(target_x_)?current+1:current-1;
            if(!act_unlocked(world,next)){cancel();return physical;}
            direction=current<unsigned(target_x_)?256:512; // R / L, never A
        } else if(mode_==Menu||mode_==DialogList) {
            int current=read(menu_selection_,scene_==23?1:4);
            if(current!=target_x_) {
                if(mode_==DialogList)direction=current<target_x_?128:64;
                else if(scene_==26) {
                    if(current/2!=target_x_/2)direction=current<target_x_?128:64;
                    else direction=current<target_x_?16:32;
                }
                else if(scene_==2)direction=target_x_==2?32:16;
                else if(scene_==17) {
                    const uint32_t world=read(0x03004898);
                    const unsigned count=object(world)?read(world+0x42,1):0;
                    if(count<1||count>8||unsigned(current)>=count||unsigned(target_x_)>=count) {
                        cancel();return physical;
                    }
                    const int column=current%4,target_column=target_x_%4;
                    if(current/4!=target_x_/4 && unsigned(target_x_/4*4+column)<count)
                        direction=current<target_x_?128:64;
                    else direction=column<target_column?16:32;
                }
                else if(scene_==22) {
                    const uint32_t w=read(0x03004898);
                    const unsigned count=object(w)?read(w+0x3d4,1):0;
                    if(count<1||count>14||unsigned(current)>=count||unsigned(target_x_)>=count) {
                        cancel();return physical;
                    }
                    const int right=(target_x_-current+count)%count,left=(current-target_x_+count)%count;
                    direction=right<=left?16:32;
                }
                else if(scene_==25)direction=target_x_==2?128:target_x_==1?32:16;
                else if(scene_==5) {
                    // Four-item wrap: hovering Options from New Game takes a
                    // single Up step, with no click or intermediate activation.
                    int down=(target_x_-current+4)%4,up=(current-target_x_+4)%4;
                    direction=down<=up?128:64;
                } else if(scene_==15 && menu_selection_==0x03004848) {
                    if(current/2!=target_x_/2)direction=current<target_x_?16:32;
                    else direction=current<target_x_?128:64;
                } else if(scene_==6) {
                    const int from=current>=7?current-1:current;
                    const int to=target_x_>=7?target_x_-1:target_x_;
                    const int count=read(0x03004848)==15?8:9;
                    direction=(to-from+count)%count<=(from-to+count)%count?128:64;
                } else if(scene_==23)direction=current<target_x_?128:64;
                else direction=current<target_x_?16:32;
            }
        }
        if(direction)pulse(direction);
        else if(pending_confirm_){
            // The native battle input samples held buttons at 03004230 and
            // new presses at 03004234 (08003e48). Two rendered neutral frames
            // need not include an input sample during selection/map work.
            if(mode_==Battle && (read(0x03004230+2*read(0x03003536,1),2)&1))return physical;
            const uint32_t p=cursor(),unit=object(p)?read(p+96):0;
            // Request native camera following only when the first A is ready,
            // after cursor travel, never merely on hover or a second confirm.
            if(mode_==Battle && (battle_action_==17||battle_action_==19) &&
               hero_click_unit_ && p==hero_click_cursor_)selection_focus_=hero_click_unit_;
            const bool open_hero=mode_==Battle && (battle_action_==18||battle_action_==20) &&
                object(unit) && target_x_==signed_read(unit+24)/65536/32 &&
                target_y_==signed_read(unit+28)/65536/32;
            const bool select_hero=mode_==Battle&&(battle_action_==17||battle_action_==19)&&hero_click_unit_&&p==hero_click_cursor_;
            pending_confirm_=false;mode_=open_hero?HeroOpen:select_hero?HeroSelect:Direct;
            if(select_hero||open_hero)started_=frame;
            if(open_hero){
                hero_open_unit_=unit;hero_open_cursor_=p;selection_focus_=unit;
                pulse_=0;return physical; // focus is applied by the runtime after update
            }
            pulse(menu_activate_);
        }
        else {
            // Keep the hovered world cell after arriving: camera follow can
            // move its sprite away from a stationary pointer before the click.
            if(mode_!=Battle||!edge_)mode_=None;
            pulse_=0;return 0x3ff;
        }
        return uint16_t(0x3ff&~pulse_);
    }
};
} // namespace gbarecomp
