// Deterministic SDL-call observer. Compile this against both baseline and
// diagnostic headers to prove diagnostic-only state does not change rendering.
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#define SDL_VERSION_ATLEAST(...) 1
struct SDL_Texture { int id; }; struct SDL_Renderer {};
struct SDL_Rect { int x,y,w,h; }; struct SDL_FRect { float x,y,w,h; };
enum { SDL_PIXELFORMAT_RGB24, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SDL_BLENDMODE_BLEND, SDL_ScaleModeNearest };
static int next_id=0,fail_create=0,fail_update=0,fail_copy=0;
SDL_Texture* SDL_CreateTexture(SDL_Renderer*,int,int,int,int){++next_id;std::cout<<"create "<<next_id<<'\n';if(fail_create&&next_id==fail_create)return nullptr;return new SDL_Texture{next_id};}
void SDL_DestroyTexture(SDL_Texture* t){std::cout<<"destroy "<<t->id<<'\n';delete t;}
int SDL_SetTextureBlendMode(SDL_Texture*,int){return 0;} int SDL_SetTextureScaleMode(SDL_Texture*,int){return 0;}
int SDL_UpdateTexture(SDL_Texture* t,const SDL_Rect*,const void* data,int pitch){uint64_t hash=1469598103934665603ull;const auto* p=static_cast<const uint8_t*>(data);for(int i=0;i<pitch*180;++i)hash=(hash^p[i])*1099511628211ull;std::cout<<"update "<<t->id<<' '<<hash<<'\n';return fail_update==t->id?-1:0;}
bool SDL_RenderIsClipEnabled(SDL_Renderer*){return false;} void SDL_RenderGetClipRect(SDL_Renderer*,SDL_Rect* p){*p={0,0,1000,1000};}
bool SDL_IntersectRect(const SDL_Rect*,const SDL_Rect*,SDL_Rect*){return true;} int SDL_RenderSetClipRect(SDL_Renderer*,const SDL_Rect*){return 0;}
int SDL_RenderCopy(SDL_Renderer*,SDL_Texture* t,const SDL_Rect*,const SDL_Rect* r){std::cout<<"copy "<<t->id<<' '<<r->x<<' '<<r->y<<' '<<r->w<<' '<<r->h<<'\n';return fail_copy==t->id?-1:0;}
int SDL_RenderCopyF(SDL_Renderer*,SDL_Texture* t,const SDL_Rect*,const SDL_FRect* r){std::cout<<"copyf "<<t->id<<' '<<r->x<<' '<<r->y<<' '<<r->w<<' '<<r->h<<'\n';return fail_copy==t->id?-1:0;}
#include "third_age_camera_present.h"
using namespace gbarecomp;
int main(){
    SDL_Renderer renderer;
    std::vector<uint8_t> world(320*180*3),mask(320*180);for(size_t i=0;i<world.size();++i)world[i]=i%251;
    ThirdAgeUnitMotionFrame unit;unit.identity=1;unit.world_x=64;unit.world_y=48;unit.left=4;unit.top=4;unit.width=2;unit.height=2;unit.mask.assign(4,1);unit.background=world;
    ThirdAgeWeatherFrame weather;weather.rgb=world;weather.mask.assign(320*180,0);weather.mask[30]=1;
    auto grade=[](const uint8_t* src,uint8_t* dst){std::copy_n(src,320*180*3,dst);};
    ThirdAgeCameraPresent p;
#ifdef DIAGNOSTIC_VERSION
    p.diagnostics.enabled=true;
#endif
    p.submit(world.data(),world.data(),mask.data(),10,1,30,40,&unit,&weather);p.upload(&renderer,grade,100000000);
    assert(p.available());assert(p.draw(&renderer,{2,3,853,480},101000000));
    unit.world_x+=3;unit.world_y+=2;p.submit(world.data(),world.data(),mask.data(),11,1,34,43,&unit,&weather);p.upload(&renderer,grade,117000000);
    assert(p.draw(&renderer,{2,3,853,480},123000000));
#ifdef DIAGNOSTIC_VERSION
    assert(p.diagnostics.upload_errors==0&&p.diagnostics.draw_errors==0);
    assert(std::abs(p.drawn_dx()*853./320-std::round(p.drawn_dx()*853./320))<1e-8);
    assert(std::abs(p.diagnostics.unit_snapped_dx*853./320-std::round(p.diagnostics.unit_snapped_dx*853./320))<1e-6);
    assert(p.diagnostics.raw_dx!=p.drawn_dx());
#endif
    p.set_scroll_input(true,true);assert(p.draw(&renderer,{2,3,853,480},127000000));
    p.set_scroll_input(false,false);assert(p.draw(&renderer,{2,3,853,480},130000000));
    fail_copy=4;assert(!p.draw(&renderer,{2,3,853,480},132000000));
#ifdef DIAGNOSTIC_VERSION
    assert(p.diagnostics.draw_errors&2048);
#endif
    fail_copy=0;p.submit(nullptr,nullptr,nullptr,12,1,34,43);assert(!p.draw(&renderer,{2,3,853,480},133000000));
#ifdef DIAGNOSTIC_VERSION
    assert(p.diagnostics.reset_flags==1&&p.diagnostics.draw_errors==1);
#endif
    p.release();
    ThirdAgeCameraPresent allocation;
#ifdef DIAGNOSTIC_VERSION
    allocation.diagnostics.enabled=true;
#endif
    fail_create=next_id+1;allocation.submit(world.data(),world.data(),mask.data(),1,1,0,0);allocation.upload(&renderer,grade,1);assert(!allocation.available());
#ifdef DIAGNOSTIC_VERSION
    assert(allocation.diagnostics.upload_errors==1);
#endif
    fail_create=0;ThirdAgeCameraPresent update;
#ifdef DIAGNOSTIC_VERSION
    update.diagnostics.enabled=true;
#endif
    fail_update=next_id+3;update.submit(world.data(),world.data(),mask.data(),1,1,0,0);update.upload(&renderer,grade,1);assert(!update.available());
#ifdef DIAGNOSTIC_VERSION
    assert(update.diagnostics.upload_errors==128&&update.diagnostics.reset_flags==256);
#endif
    update.release();
}
