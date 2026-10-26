#include <cstdio>
#include "runtime.h"

// Anchor the BIOS table in the executable so ELF static-library linking pulls
// it in before resolving the ARM runtime's references.
extern "C" const unsigned kBiosDispatchTableLen;

int main(int argc, char** argv) {
    std::fprintf(stderr,
        "Third Age GBA PC — experimental boot trial.\n"
        "Static coverage and gameplay are not yet verified; inspect runtime diagnostics.\n");
    std::fprintf(stderr, "Compiled BIOS dispatch entries: %u\n", kBiosDispatchTableLen);
    gbarecomp::RunOptions options;
    options.builtin_game_name = "The Third Age (GBA) — Experimental";
    options.builtin_rom_sha1 = "0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54";
    options.freely_resizable_window = true;
    return gbarecomp::run_game(argc, argv, options);
}
