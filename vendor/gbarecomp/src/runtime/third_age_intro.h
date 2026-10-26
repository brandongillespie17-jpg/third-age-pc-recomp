#pragma once
#include <cstdint>

namespace gbarecomp {
// Exact-ROM startup adapter. Uses ordinary button edges and host fast-forward;
// never jumps the guest PC, changes scene RAM, or replaces save initialization.
class ThirdAgeIntro {
    bool boot_, closed_=false, active_=false, epoch_known_=false;
    uint64_t epoch_=0, start_=0;
    uint32_t scene_=0, pending_=0;
    static bool intro(uint32_t scene) {
        return scene==3 || scene==4 || scene==13 || scene==29 || scene==31;
    }
public:
    explicit ThirdAgeIntro(bool fresh_boot):boot_(fresh_boot) {}
    void observe(uint32_t scene,uint32_t pending,uint64_t frame,uint64_t epoch) {
        if(epoch_known_ && epoch!=epoch_) {
            // Loading/rewinding may enter gameplay. Never carry a boot request
            // into the restored scene or interpret uninitialized RAM as a menu.
            boot_=false;closed_=false;active_=false;
        }
        epoch_known_=true;epoch_=epoch;scene_=scene;pending_=pending;
        if(intro(scene))boot_=false;
        else if(!boot_ || (scene>0 && scene<=31))closed_=true;
        if(closed_ || (active_ && (frame<start_ || frame-start_>=3600)))active_=false;
    }
    bool available() const {return !closed_ && (boot_ || intro(scene_));}
    void request(uint64_t frame) {
        if(available() && !active_) {active_=true;start_=frame;}
    }
    void cancel() {active_=false;}
    bool active() const {return active_;}
    uint16_t keys(uint64_t frame) const {
        // Release during scene changes; Start cannot select a new campaign.
        // Six-frame presses also reach the game's slower menu polling loops.
        if(!active_ || !intro(scene_) || pending_!=scene_ || frame<start_ ||
           (frame-start_)%24>=6)return 0x3ff;
        return scene_==13 ? 0x3fe : 0x3f7;
    }
};
}
