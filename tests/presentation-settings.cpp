// Run with SDL's dummy video/audio drivers; never opens the user's desktop.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "host_window.h"
#include "host_platform.h"
#include "third_age_overlay.h"
using namespace gbarecomp;
extern "C" const unsigned kBiosDispatchTableLen;
static void set_environment(const char* name,const char* value) {
    assert(SDL_setenv(name,value,1)==0);
#ifdef _WIN32
    // SDL's DLL and the statically linked executable have separate CRT
    // environment tables. The runtime reads std::getenv from this executable.
    assert(_putenv_s(name,value)==0);
#endif
}
static void key(HostWindow& win, SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=code;
    assert(SDL_PushEvent(&e)==1);win.pump();
    e.type=SDL_KEYUP;assert(SDL_PushEvent(&e)==1);win.pump();
}
int main(int argc,char** argv) {
    SDL_SetMainReady();
    assert(kBiosDispatchTableLen==0);
    assert(argc==2);const std::filesystem::path dir=argv[1];
    std::filesystem::create_directories(dir);
    {std::ofstream legacy(dir/"third-age-settings.ini");legacy<<"volume=37\nedge_speed=5\n";}
    ThirdAgeSettings settings;settings.load(dir/"third-age-settings.ini");
    assert(!settings.camera_smoothing&&settings.presentation_hz==165&&!settings.sprite_cache);
    // Legacy settings remain valid; unrelated fields survive every save/reload.
    settings.volume=37;settings.edge_speed=5;settings.expanded_view=true;
    // Snapshot defaults, independence from display/audio/gameplay, and custom
    // values surviving OFF -> save -> cold reload -> ON.
    ThirdAgeSettings preset;
    assert(preset.hd_enabled&&preset.expanded_view&&preset.hd_unit_panel&&preset.hd_portraits);
    assert(preset.panel_theme.colors[ThirdAgePanelTheme::BackgroundTop]==0x00100d);
    assert(preset.panel_theme.colors[ThirdAgePanelTheme::HealthFill]==0x18db18);
    preset.path=dir/"preset.ini";preset.resolution=3;preset.fullscreen=true;
    preset.ui_scale=125;preset.volume=37;preset.skip_combat=true;preset.presentation_hz=120;
    preset.smooth=true;preset.hd_battle_menus=false;
    preset.panel_theme.colors[ThirdAgePanelTheme::HealthFill]=0x123456;
    preset.set_hd_enabled(false);assert(!preset.expanded_view&&!preset.hd_portraits&&!preset.blend);
    assert(preset.save());ThirdAgeSettings off;off.load(preset.path);
    assert(!off.hd_enabled&&!off.expanded_view&&!off.hd_unit_panel&&!off.hd_portraits);
    off.set_hd_enabled(true);assert(off.smooth&&!off.hd_battle_menus&&off.expanded_view);
    assert(off.panel_theme.colors[ThirdAgePanelTheme::HealthFill]==0x123456);
    assert(off.resolution==3&&off.fullscreen&&off.ui_scale==125&&off.volume==37&&off.skip_combat&&off.presentation_hz==120);
    assert(off.save());ThirdAgeSettings on;on.load(preset.path);
    assert(on.hd_enabled&&on.smooth&&!on.hd_battle_menus);
    assert(on.panel_theme==off.panel_theme);
    ThirdAgeRefreshClock clock(0);
    assert(!clock.enabled());assert(clock.set_hz(165));assert(clock.due(100));
    clock.presented(100);assert(clock.deadline_ns()==6060706);assert(!clock.due(101));
    assert(!clock.set_hz(165));assert(clock.deadline_ns()==6060706);
    assert(clock.set_hz(60));assert(clock.deadline_ns()==0);assert(clock.due(101));
    assert(clock.set_hz(0));assert(!clock.enabled());assert(clock.due(101));
    // A launch with smoothing OFF must still service intermediate presents
    // after switching ON. Turning it OFF restores the ordinary sleeping path.
    int service_calls=0;FramePacer pacer(60);
    pacer.set_wait_service([&](){++service_calls;},[](){return third_age_present_hz()!=0;});
    third_age_set_present_hz(0);pacer.reset();pacer.wait_for_next_frame();assert(service_calls==0);
    third_age_set_present_hz(165);pacer.reset();pacer.wait_for_next_frame();assert(service_calls>0);
    service_calls=0;third_age_set_present_hz(0);pacer.reset();pacer.wait_for_next_frame();assert(service_calls==0);
    hd_font::initialize_external_font(std::filesystem::path(argv[0]));
    HostWindow win;assert(win.open());win.set_third_age_settings(&settings);
    assert(third_age_present_hz()==0);
    key(win,SDLK_F10);key(win,SDLK_TAB); // Video: master and Advanced.
    key(win,SDLK_RETURN);assert(!settings.hd_enabled&&!settings.expanded_view);
    key(win,SDLK_DOWN);key(win,SDLK_RETURN); // Advanced is disabled while OFF.
    assert(!settings.hd_enabled);
    key(win,SDLK_UP);key(win,SDLK_RETURN);assert(settings.hd_enabled&&settings.expanded_view);
    key(win,SDLK_DOWN);key(win,SDLK_RETURN); // Enter Advanced, toggle blending.
    const bool old_blend=settings.blend;key(win,SDLK_RETURN);assert(settings.blend!=old_blend);
    key(win,SDLK_ESCAPE);key(win,SDLK_UP);key(win,SDLK_RETURN);key(win,SDLK_RETURN);
    assert(settings.hd_enabled&&settings.blend!=old_blend);
    for(int i=0;i<3;++i)key(win,SDLK_TAB);
    for(int i=0;i<4;++i)key(win,SDLK_DOWN);
    key(win,SDLK_RETURN); // Real overlay event -> save -> live host/pacer update.
    assert(settings.camera_smoothing&&third_age_present_hz()==165);
    key(win,SDLK_DOWN);key(win,SDLK_LEFT);
    assert(settings.presentation_hz==144&&third_age_present_hz()==144);
    key(win,SDLK_LEFT);assert(third_age_present_hz()==120);
    key(win,SDLK_LEFT);key(win,SDLK_LEFT);assert(third_age_present_hz()==60);
    key(win,SDLK_RIGHT);key(win,SDLK_RIGHT);key(win,SDLK_RIGHT);key(win,SDLK_RIGHT);
    assert(third_age_present_hz()==165);
    key(win,SDLK_LEFT); // Keep 144 Hz as the remembered choice.
    key(win,SDLK_UP);key(win,SDLK_SPACE);
    assert(!settings.camera_smoothing&&third_age_present_hz()==0);
    key(win,SDLK_DOWN);key(win,SDLK_LEFT); // Disabled rate row cannot change.
    assert(settings.presentation_hz==144);
    key(win,SDLK_UP);key(win,SDLK_SPACE);
    assert(settings.camera_smoothing&&third_age_present_hz()==144);
    // The new Gameplay row stays navigable without moving existing controls.
    for(int i=0;i<4;++i)key(win,SDLK_TAB);
    for(int i=0;i<6;++i)key(win,SDLK_DOWN);key(win,SDLK_RETURN);
    assert(settings.sprite_cache);
    ThirdAgeSettings loaded;loaded.load(settings.path);
    assert(loaded.camera_smoothing&&loaded.presentation_hz==144&&loaded.sprite_cache);
    assert(loaded.volume==37&&loaded.edge_speed==5);
    // Saved choices override the legacy diagnostic launcher defaults.
    set_environment("THIRD_AGE_PRESENT_HZ","60");set_environment("THIRD_AGE_DITHER_OAM_CACHE","0");
    ThirdAgeSettings seeded;assert(seeded.camera_smoothing&&seeded.presentation_hz==60&&!seeded.sprite_cache);
    seeded.load(settings.path);assert(seeded.camera_smoothing&&seeded.presentation_hz==144&&seeded.sprite_cache);
    // Reject malformed/unsupported persisted values without damaging defaults.
    {std::ofstream f(dir/"invalid.ini");f<<"camera_smoothing=2\npresentation_hz=999\nsprite_cache=-1\n";}
    seeded.load(dir/"invalid.ini");assert(seeded.camera_smoothing&&seeded.presentation_hz==144&&seeded.sprite_cache);
    set_environment("THIRD_AGE_PRESENT_HZ","");set_environment("THIRD_AGE_DITHER_OAM_CACHE","");
    // Recreating a window restores the saved choice, including a cold clock.
    win.close();HostWindow reopened;assert(reopened.open());reopened.set_third_age_settings(&loaded);
    assert(third_age_present_hz()==144);
    loaded.camera_smoothing=false;reopened.apply_third_age_settings();assert(third_age_present_hz()==0);
    reopened.close();
    // Render real overlay geometry for visual review, using the release font.
    auto* surface=SDL_CreateRGBSurfaceWithFormat(0,1440,960,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
    auto* renderer=SDL_CreateSoftwareRenderer(surface);assert(renderer);
    {
        ThirdAgeOverlay overlay;overlay.settings=&loaded;overlay.open=true;overlay.display.edit(loaded);
        overlay.tab=1;overlay.row=0;
        SDL_RenderSetScale(renderer,2,2);overlay.draw_contents(renderer);SDL_RenderPresent(renderer);
        assert(SDL_SaveBMP(surface,(dir/"hd-preset.bmp").string().c_str())==0);
        overlay.advanced_video=true;overlay.draw_contents(renderer);SDL_RenderPresent(renderer);
        assert(SDL_SaveBMP(surface,(dir/"hd-advanced.bmp").string().c_str())==0);
        loaded.camera_smoothing=true;overlay.tab=4;overlay.row=4;
        SDL_RenderSetScale(renderer,2,2);overlay.draw_contents(renderer);SDL_RenderPresent(renderer);
        assert(SDL_SaveBMP(surface,(dir/"display.bmp").string().c_str())==0);
        overlay.tab=3;overlay.row=6;overlay.draw_contents(renderer);SDL_RenderPresent(renderer);
        assert(SDL_SaveBMP(surface,(dir/"gameplay.bmp").string().c_str())==0);
        // Real hit testing at 1440x960, using the overlay's normal scaling.
        auto* window=SDL_CreateWindow("test",0,0,1440,960,SDL_WINDOW_HIDDEN);assert(window);
        SDL_RenderSetScale(renderer,1,1);overlay.tab=4;overlay.row=4;
        SDL_Event click{};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;
        click.button.x=720;click.button.y=480; // Camera smoothing row center at 75% UI scale.
        const float scale=ThirdAgeDisplay::overlay_scale(1440,960,loaded.ui_scale);
        click.button.y=int((960-480*scale)/2+(158+4*24+11)*scale);
        overlay.event(click,renderer,window);assert(!loaded.camera_smoothing);
        SDL_DestroyWindow(window);
    }
    SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    std::cout<<"PASS: live keyboard/mouse controls, all rates, off/on, persistence, legacy flags, validation, window recreation, menu renders\n";
}
