# The Third Age GBA PC recomp — source preparation

Preserved hybrid GBA runtime and PC presentation/control enhancements, based on the October 15, 2026 camera-weather campaign checkpoint. This is not a complete native/static port. The separate limited PC-native Amon Hen prototype is not merged.

**A fresh Linux build from owned ROM, BIOS and the original local font now succeeds. Windows compilation and runtime validation remain pending the reviewer.** No executable or private game/font/art data is distributed. Existing generated source and old build caches are not required.

Portable Windows release work now has a separate `THIRD_AGE_PORTABLE_RELEASE=ON` build. It needs no private assets during compilation and loads the owned ROM/BIOS at startup, derives portrait/decoder data in memory, and uses a system font with optional local Ringbearer. Linux boot and asset regressions pass; Windows compilation and native gameplay validation are pending. See [release validation](RELEASE-VALIDATION.json) and [Windows release build](docs/WINDOWS-RELEASE.md).

Read [build instructions](docs/BUILD.md), [Windows reviewer checklist](docs/WINDOWS-REVIEWER.md), [licensing inventory](LICENSING-INVENTORY.md), [roadmap](docs/ROADMAP.md), and [preparation changes](PREPARATION-CHANGES.md). The separate corrected recovery archive preserves the original roadmap, dated evidence and baseline source.

This source-review snapshot is prepared for the public repository [brandongillespie17-jpg/third-age-pc-recomp](https://github.com/brandongillespie17-jpg/third-age-pc-recomp). The staged content audit is complete and the owner has authorized public source review.

No additional project-wide license is granted. Existing framework and component licenses and notices remain in force, including the framework's PolyForm Noncommercial terms. Project-specific licensing and provenance choices remain documented in the licensing inventory. Publication is not a claim of legal clearance or unrestricted open-source rights and does not override third-party rights. The original private-input build requires a local original Ringbearer TTF to preserve baseline typography; the portable release path uses an installed system font by default. HD artwork is optional and external.
