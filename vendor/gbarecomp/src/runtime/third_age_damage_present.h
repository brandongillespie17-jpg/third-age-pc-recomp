#pragma once
#include "third_age_refresh_clock.h"
#include "third_age_combat.h"
namespace gbarecomp {
// Interpolate the existing host-owned floating-number age. Damage values and
// lifetime remain guest-owned; only vertical position/fade use fractional age.
class ThirdAgeDamagePresent {
    std::vector<ThirdAgeDamageNumber> previous_;
    std::vector<double> from_;
    int64_t stamp_=0;
public:
    void update(const std::vector<ThirdAgeDamageNumber>& next,int64_t now=third_age_present_now()){
        bool same=next.size()==previous_.size();
        if(same)for(size_t i=0;i<next.size();++i){const auto&a=next[i];const auto&b=previous_[i];
            if(a.age!=b.age||a.loss!=b.loss||a.miss!=b.miss||a.x+a.camera_x!=b.x+b.camera_x||a.y+a.camera_y!=b.y+b.camera_y){same=false;break;}}
        if(same)return;
        from_.resize(next.size());
        for(size_t i=0;i<next.size();++i){const auto& n=next[i];from_[i]=n.age;
            if(i<previous_.size()){const auto& p=previous_[i];
                if(n.loss==p.loss&&n.miss==p.miss&&n.age==p.age+1&&
                   n.x+n.camera_x==p.x+p.camera_x&&n.y+n.camera_y==p.y+p.camera_y)
                    from_[i]=p.age;
            }
        }
        previous_=next;stamp_=now;
    }
    double age(size_t index,int native_age,int64_t now=third_age_present_now())const{
        if(index>=from_.size())return native_age;
        const double alpha=std::clamp(double(now-stamp_)/ThirdAgeCameraTween::guest_ns,0.,1.);
        return from_[index]+(native_age-from_[index])*alpha;
    }
};
}
