# Changes relative to recovered campaign source

- Replaced stale top-level README with accurate preparation/build status; retained original documents privately in the recovery archive.
- Retained modified vendor source and changed `.gitignore` so it is not hidden. Added private-input, save, binary and cache exclusions.
- Split the original `third_age_native_portraits.h`: rendering/matching logic remains in public source; the two ROM-derived pixel-signature functions become a private include. The exact verified migration tool preserves all data bytes locally. Font bytes are also private. No HD-art feature is removed or stubbed.
- Added `THIRD_AGE_PRIVATE_INCLUDE_DIR` and explicit missing-input errors in root CMake. Invoked the existing MinGW runtime DLL staging helper.
- Added a Python local-input configure/build route. Removed three obsolete clean-upstream Bash bootstrap scripts from this preparation copy; byte-exact originals remain in the recovery archive.
- Made the trial launcher choose the Windows `.exe` or `Release` path and explicitly use the recorded interpreter/no-self-heal/no-RAM-overlay campaign policy.
- Added distribution, build and reconstructed roadmap documentation. No Windows binary, clean-ROM bootstrap, native Windows gameplay or public repository publication is claimed.

The limited demo was not merged. Original source directories and the recovery archive remain unchanged.

## Clean-generation follow-up (v3)

- Added a portable clean build from owned ROM/BIOS plus locally supplied original TTF. It compiles the recompiler, generates game/BIOS C++ and all private headers afresh, then builds the runtime without network fetching.
- Replaced the checkpoint-header migration with a ROM resource decoder. All 136 portrait pixel/signature orders match the baseline exactly.
- Removed the inline ROM-derived graphics instruction body from public source and added an ARM-subset generator; the private 203-instruction output matches the baseline body byte-for-byte.
- Vendored pinned toml++ v3.4.0 source and MIT license for offline builds.
- Completed a fresh Linux compile/link using no prior generated game/BIOS sources or build cache. No game was launched; Windows validation is pending. See BUILD-VALIDATION.json and LICENSING-INVENTORY.md.
