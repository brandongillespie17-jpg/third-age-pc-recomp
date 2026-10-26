#pragma once
#include <cstdint>

namespace gbarecomp {
// Exact-ROM combat presentation shortcuts. The caller gates these by ROM hash,
// interpreter mode, the user's preference, and an active skip request. Never
// select an outcome, set an animation-complete flag, or jump to another scene:
// all script commands and combat callbacks still execute in their original order.
struct ThirdAgeFastCombat {
    enum class Action { None, ExpirePause, ShortenFrame };
    static Action action(uint32_t pc, const uint32_t* r, const uint8_t* iw,
                         const uint8_t* ew) {
        if (pc != 0x08007a3cu && pc != 0x08007d6cu && pc != 0x08002764u)
            return Action::None;
        if (!iw || !ew) return Action::None;
        auto word=[](const uint8_t* p) {
            return uint32_t(p[0]) | uint32_t(p[1])<<8 |
                   uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
        };
        auto object=[](uint32_t a) {
            return a>=0x02000000u && a<=0x0203fec0u && !(a&3);
        };
        if (word(iw+0x4840)!=8) return Action::None;
        const auto world=word(iw+0x4898), actor=r[5];
        if (!object(world) || !object(actor)) return Action::None;
        const auto* w=ew+world-0x02000000u;
        // Only the combat clones and the native temporary attack actor.
        if (actor!=word(w+0x20) && actor!=word(w+0x24) && actor!=word(w+0x28))
            return Action::None;
        const auto* a=ew+actor-0x02000000u;
        if (word(a+0xbc)!=0x08007995u) return Action::None;
        if (pc==0x08002764u) {
            // STRB R0,[R4] follows the native sprite-script duration load.
            // Keep one update per cel so script events cannot be collapsed.
            // A zero-duration/special cel retains its original semantics.
            return r[0]>1 && r[0]<=255 && r[4]==actor+0x114
                ? Action::ShortenFrame : Action::None;
        }
        // These two LDR sites consume presentation countdowns at actor+58:
        // the wind-up (state 17 hex) and post-result/departure (18/19 hex).
        const bool state_ok=pc==0x08007a3cu ? a[0x4e]==0x17
                                          : (a[0x4e]==0x18 || a[0x4e]==0x19);
        const auto timer=word(a+0x58);
        return state_ok && timer>0 && timer<=45 ? Action::ExpirePause : Action::None;
    }
};
} // namespace gbarecomp
