#include <cstdio>
#include <cstdlib>
#include "runtime.h"
#include "utf8_path.h"
#include <vector>
#include <string>

// Anchor the BIOS table in the executable so ELF static-library linking pulls
// it in before resolving the ARM runtime's references.
extern "C" const unsigned kBiosDispatchTableLen;

int third_age_main(int argc, char** argv) {
#ifdef THIRD_AGE_PORTABLE_RELEASE
    if(kBiosDispatchTableLen!=0){std::fprintf(stderr,"Release contains unexpected generated BIOS dispatch.\n");return 1;}
    // Never invoke a compiler on the player's computer.
#ifdef _WIN32
    _putenv_s("GBARECOMP_SELFHEAL_RECOMPILE","0");
    _putenv_s("GBARECOMP_RAM_OVERLAY_HEAL","0");
#else
    setenv("GBARECOMP_SELFHEAL_RECOMPILE","0",1);
    setenv("GBARECOMP_RAM_OVERLAY_HEAL","0",1);
#endif
#endif
#ifdef THIRD_AGE_PORTABLE_RELEASE
    std::fprintf(stderr,"Third Age PC — review build. Select your owned ROM and BIOS when prompted.\n");
#else
    std::fprintf(stderr,
        "Third Age GBA PC — experimental boot trial.\n"
        "Static coverage and gameplay are not yet verified; inspect runtime diagnostics.\n");
    std::fprintf(stderr, "Compiled BIOS dispatch entries: %u\n", kBiosDispatchTableLen);
#endif
    gbarecomp::RunOptions options;
    options.builtin_game_name = "The Third Age (GBA) — Experimental";
    options.builtin_rom_sha1 = "0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54";
    options.freely_resizable_window = true;
    return gbarecomp::run_game(argc, argv, options);
}

#if defined(_WIN32) && defined(THIRD_AGE_PORTABLE_RELEASE)
int wmain(int argc,wchar_t** argv){
    std::vector<std::string> values;for(int i=0;i<argc;++i)values.push_back(gbarecomp::path_to_utf8(std::filesystem::path(argv[i])));
    std::vector<char*> args;for(auto& s:values)args.push_back(s.data());args.push_back(nullptr);
    return third_age_main(argc,args.data());
}
#else
int main(int argc,char** argv){return third_age_main(argc,argv);}
#endif
