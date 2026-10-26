#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace gbarecomp {
struct ThirdAgeDamageNumber {
    int x=0,y=0,loss=0,age=0;
    bool miss=false;
    int camera_x=0,camera_y=0; // origin used when this result was sampled
    void rebase_camera(int cx,int cy){
        // Keep world position invariant when publishing a completed frame.
        x+=camera_x-cx;y+=camera_y-cy;camera_x=cx;camera_y=cy;
    }
};
// Exact-ROM, read-only presentation adapter. Battlefield objects survive combat
// outside the active object list; scene 8 uses a separate list of combat sprites.
class ThirdAgeCombat {
    struct Unit { uint32_t address=0;unsigned kind=0,id=0;int hp=0,last_hp=0,x=0,y=0; };
    const uint8_t* iw_=nullptr;const uint8_t* ew_=nullptr;
    std::vector<Unit> baseline_,fight_;
    std::vector<Unit> misses_;
    struct Pending {uint32_t address;unsigned kind,id;int x,y,loss,age;bool miss=false;};
    std::vector<Pending> pending_;
    uint64_t epoch_=~uint64_t{0},frame_=~uint64_t{0};
    bool in_combat_=false,skipped_=false,one_shot_=false,returning_=false;
    unsigned return_frames_=0;
    unsigned ready_frames_=0;
    uint32_t read(uint32_t a,unsigned n=4)const {
        const uint8_t* p=nullptr;
        if(iw_&&a>=0x03000000&&uint64_t(a)+n<=0x03008000)p=iw_+a-0x03000000;
        if(ew_&&a>=0x02000000&&uint64_t(a)+n<=0x02040000)p=ew_+a-0x02000000;
        uint32_t v=0;if(p)for(unsigned i=0;i<n;++i)v|=uint32_t(p[i])<<(8*i);return v;
    }
    static bool object(uint32_t p){return p>=0x02000000&&p<=0x0203fec0&&!(p&3);}
    bool identity(const Unit& u)const{return object(u.address)&&read(u.address+8,1)==u.kind&&read(u.address+10,2)==u.id;}
    Unit unit(uint32_t p)const {
        Unit u;u.address=p;u.kind=read(p+8,1);u.id=read(p+10,2);
        // 08008238 subtracts damage from signed byte +87; overkill may be
        // negative. 08040a10 computes its maximum. Never display overkill as HP.
        u.hp=u.last_hp=std::max(0,int(int8_t(read(p+0x87,1))));
        u.x=int32_t(read(p+24))/65536;u.y=int32_t(read(p+28))/65536;return u;
    }
    std::vector<Unit> battlefield()const {
        std::vector<Unit> result;uint32_t p=read(0x03002e40);
        for(unsigned i=0;i<512&&object(p);++i){
            const unsigned kind=read(p+8,1);
            if((kind==5||kind==6)&&(read(p+12)&0x10))result.push_back(unit(p));
            const uint32_t next=read(p);if(next==p)break;p=next;
        }
        return result;
    }
public:
    std::vector<ThirdAgeDamageNumber> display;
    void reset(){baseline_.clear();fight_.clear();misses_.clear();pending_.clear();display.clear();in_combat_=skipped_=one_shot_=returning_=false;return_frames_=ready_frames_=0;frame_=~uint64_t{0};}
    void note_miss(const uint8_t* iw,const uint8_t* ew,uint32_t target,uint64_t epoch){
        iw_=iw;ew_=ew;
        if(epoch!=epoch_){reset();epoch_=epoch;}
        // Called only at the exact-ROM native result-label routine, with the
        // same zero argument that selects text 014E (English "Miss!"). No HP
        // delta or hit-chance inference is used to classify the outcome.
        if(read(0x03004840)!=8||!object(target))return;
        const auto u=unit(target);if(u.kind!=5&&u.kind!=6)return;
        if(misses_.size()<128&&std::none_of(misses_.begin(),misses_.end(),[&](const Unit& n){return n.address==target&&n.kind==u.kind&&n.id==u.id;}))misses_.push_back(u);
    }
    void request_skip(){if(in_combat_)one_shot_=true;}
    bool one_shot()const{return one_shot_;}
    bool in_combat()const{return in_combat_;}
    void observe(const uint8_t* iw,const uint8_t* ew,uint64_t frame,uint64_t epoch,bool auto_skip,bool numbers,bool picture_ready=true){
        iw_=iw;ew_=ew;
        if(epoch!=epoch_|| (frame_!=~uint64_t{0}&&frame<frame_)){reset();epoch_=epoch;}
        if(frame==frame_)return;
        frame_=frame;
        const auto scene=read(0x03004840);
        if(scene==8){
            display.clear();
            if(!in_combat_){
                fight_=baseline_;in_combat_=true;skipped_=false;one_shot_=false;
                // Loading a state inside combat has no pre-fight history. Use
                // the original actor pointers and report only subsequent loss.
                if(fight_.empty()){
                    const auto w=read(0x03004898);
                    if(object(w))for(unsigned off:{0x18u,0x1cu}){
                        const auto p=read(w+off);
                        if(object(p)&&(read(p+8,1)==5||read(p+8,1)==6)&&
                           std::none_of(fight_.begin(),fight_.end(),[&](const Unit& u){return u.address==p;}))fight_.push_back(unit(p));
                    }
                }
            }
            skipped_=skipped_||auto_skip||one_shot_;
            for(auto& u:fight_)if(identity(u))u.last_hp=unit(u.address).hp;
            return;
        }
        if(in_combat_){
            for(auto& u:fight_)if(identity(u))u.last_hp=unit(u.address).hp;
            if(skipped_&&numbers)for(const auto& u:fight_)if(u.hp>u.last_hp)
                pending_.push_back({u.address,u.kind,u.id,u.x,u.y,u.hp-u.last_hp,0});
            if(skipped_&&numbers)for(const auto& u:misses_)
                pending_.push_back({u.address,u.kind,u.id,u.x,u.y,0,0,true});
            fight_.clear();baseline_.clear();misses_.clear();in_combat_=skipped_=one_shot_=false;
            returning_=true;return_frames_=ready_frames_=0;
        }
        display.clear();
        if(!numbers)pending_.clear();
        if(scene!=1){
            // Keep results across short battle messages, but never carry them
            // into a menu, another map, or the next loaded save.
            if(scene!=2){pending_.clear();baseline_.clear();returning_=false;}
            return;
        }
        auto live=battlefield();
        if(returning_){
            ++return_frames_;
            if(picture_ready)++ready_frames_;else ready_frames_=0;
            if(read(0x03004880)!=1||live.empty()||ready_frames_<2){
                if(return_frames_>120){pending_.clear();returning_=false;}
                return;
            }
            returning_=false;
        }
        if(!live.empty())baseline_=live;
        const unsigned player=read(0x03003536,1);if(player>=2)return;
        const int cx=int32_t(read(0x03001bb0+player*8)),cy=int32_t(read(0x03001bb4+player*8));
        if(cx< -8192||cx>8192||cy< -8192||cy>8192)return;
        for(auto& n:pending_){
            for(const auto& u:live)if(u.address==n.address&&u.kind==n.kind&&u.id==n.id){n.x=u.x;n.y=u.y;break;}
            const bool stacked=n.miss&&std::any_of(pending_.begin(),pending_.end(),[&](const Pending& p){return !p.miss&&p.address==n.address;});
            if(n.age<90)display.push_back({n.x-cx,n.y-cy-32-(stacked?12:0),n.loss,n.age,n.miss,cx,cy});
            ++n.age;
        }
        pending_.erase(std::remove_if(pending_.begin(),pending_.end(),[](const Pending& n){return n.age>=90;}),pending_.end());
    }
};
}
