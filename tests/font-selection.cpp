#define THIRD_AGE_PORTABLE_RELEASE
#include "third_age_hd_font.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
    assert(argc==2);
    const std::filesystem::path root=argv[1];
    const auto selected=gbarecomp::hd_font::initialize_external_font(root/"third-age-pc.exe");
    assert(std::filesystem::equivalent(gbarecomp::path_from_utf8(selected),root/"assets/fonts/ringbearer.ttf"));
    assert(gbarecomp::hd_font::glyphs['L'-32].count>0);
    assert(gbarecomp::hd_font::glyphs['o'-32].count>0);
    assert(!gbarecomp::hd_font::edges.empty());
    std::cout<<"PASS: packaged Ringbearer selected automatically; glyph outlines loaded: "<<selected<<"\n";
}
