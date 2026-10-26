# The Third Age GBA PC recomp

**[Windows x64 ZIP](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/download/windows-preview-hd-2026-10-26/third-age-pc-windows-x64-preview-4beaa8d235b3.zip)** · [Windows release notes](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/tag/windows-preview-hd-2026-10-26)

**[Linux x86_64 tar.gz](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/download/linux-preview-hd-2026-10-26/third-age-pc-linux-x86_64-preview-4beaa8d235b3.tar.gz)** · [Linux release notes](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/tag/linux-preview-hd-2026-10-26)

Extract into a new writable folder. Windows: run `third-age-pc.exe` and select your own matching GBA ROM and BIOS; SDL2 is included. Linux: Ubuntu 24.04 or compatible newer distributions, glibc 2.39+, SDL2 and a system TrueType font; run `./play.sh --bios "/absolute/path/gba_bios.bin" --rom "/absolute/path/game.gba"`. Later launches remember validated input paths. No compiler, Python or Wine is needed to play either native build. See [Windows setup](docs/WINDOWS-REVIEWER.md) or [Linux setup](docs/LINUX-RELEASE.md).

**F10 / Escape → Video → HD Preset** toggles the captured expanded battlefield, HD panels, portraits and custom colors. **Advanced** exposes individual visual choices. Turning HD off/on remembers customizations and leaves resolution, fullscreen, UI size, refresh rate, audio, controls and gameplay preferences unchanged. Existing settings retain their individual choices. Captured values: [docs/HD-PRESET.json](docs/HD-PRESET.json).

**Display → Camera Smoothing** applies immediately and persists. **Presentation Rate** offers 60/120/144/165 Hz. Camera interpolation uses the expanded battlefield. **Gameplay → Sprite Cache** independently controls the experimental renderer cache. The simulation keeps its normal speed.

Both releases include the same 68 approved portraits and original Ringbearer archive, with notices. The corrected Windows package loads Ringbearer automatically from assets/fonts/ringbearer.ttf and includes all original font files. The existing Linux package keeps its system-font default and optional font archive. No ROM, BIOS or personal save is included. Each release has an archive checksum and per-file hashes.

Windows and Linux compiled and passed the actual SDL overlay/preset/persistence regression with dummy drivers in [CI](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40405711931), built from `4beaa8d235b3e40549c675fceb1d0218431459e9`. The Windows dependency-loader and both archive content/checksum audits passed. Physical display smoothness, audio devices and full-campaign gameplay remain untested by CI; native Windows gameplay acceptance remains pending. The separately [completed publication run](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40406576676) verified both uploaded archives.

This preserves the hybrid GBA runtime and PC presentation/control enhancements from the October 15 camera-weather checkpoint. It is not a complete native/static port. The separate limited PC-native Amon Hen prototype is not merged.

Source builds are optional: see the [portable Windows build and package guide](docs/WINDOWS-RELEASE.md) or the [private-input source build instructions](docs/BUILD.md). The portable build loads owned inputs at runtime; the original private-input build has different developer requirements. Earlier validation JSON files remain historical records of the checks available when they were written.

See the [roadmap](docs/ROADMAP.md), [preparation changes](PREPARATION-CHANGES.md), [licensing inventory](LICENSING-INVENTORY.md), [portrait provenance and rights notice](assets/PORTRAIT-NOTICE.md), and [Ringbearer notice](assets/FONT-NOTICE.md).

No additional project-wide license is granted. Existing framework and component licenses and notices remain in force, including the framework's PolyForm Noncommercial terms. Project-specific licensing and provenance choices remain documented in the licensing inventory. Portrait and font rights are separate from code licenses. Publication is not a claim of legal clearance or unrestricted open-source rights and does not override third-party rights.
