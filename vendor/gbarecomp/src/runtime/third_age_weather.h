#pragma once
#include <cstdint>
#include <vector>
namespace gbarecomp {
// Verified opaque, screen-owned weather. Native raster positions and colors
// are retained; this layer never receives the battlefield camera displacement.
struct ThirdAgeWeatherFrame {
    std::vector<uint8_t> rgb,mask,foreground_rgb,foreground;
    bool valid()const{return rgb.size()==320*180*3&&mask.size()==320*180;}
    void clear(){rgb.clear();mask.clear();foreground_rgb.clear();foreground.clear();}
};
}
