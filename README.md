# The Third Age GBA PC recomp

**[Download the Windows x64 preview ZIP](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/download/windows-preview-2026-10-26/third-age-pc-windows-x64-preview-b8b7714f5a38.zip)** · [Release notes](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/tag/windows-preview-2026-10-26)

Extract the ZIP into a writable folder, double-click `third-age-pc.exe`, and select your own matching GBA ROM and BIOS. **No compiler, Python, CMake or dependency installer is needed.** See the [Windows setup and reviewer checklist](docs/WINDOWS-REVIEWER.md).

The October 26, 2026 preview includes the executable, SDL2 runtime, 68 unchanged approved portraits, and the optional complete original Ringbearer archive. A system font is the default. ROM/BIOS dumps, generated game data and personal saves are excluded. Bulky assets are distributed through the release ZIP; this repository contains source and metadata.

ZIP: **257,616,570 bytes**. SHA-256:

```text
6a8086fae52a2246c336826b2171bec11ef800b5e9a9dfe076902625c45d3356
```

Windows compilation, dependency-loader checks and ZIP auditing passed in [CI run 40383530854](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40383530854), built from commit `b8b7714f5a38032dcc51cf6355ea6d4189b68006`. A bounded 300-frame Wine boot/save test passed, matching the Linux reference framebuffer, save, IWRAM and IRQ/SWI logs. **Native Windows gameplay validation remains pending the reviewer.** The Wine check used an installed local font and is not proof of native GUI, audio, controls or full-campaign correctness.

This preserves the hybrid GBA runtime and PC presentation/control enhancements from the October 15 camera-weather campaign checkpoint. It is not a complete native/static port. The separate limited PC-native Amon Hen prototype is not merged.

The next build adds **F10 / Escape → Display → Camera Smoothing**, with a
60/120/144/165 Hz presentation-rate selector. Camera interpolation uses the
expanded battlefield. **Gameplay → Sprite Cache** separately controls the
experimental rendering cache. These preferences apply immediately and save
between launches. They are not in the previously linked October 26 executable.

Source builds are optional: see the [portable Windows build and package guide](docs/WINDOWS-RELEASE.md) or the [private-input source build instructions](docs/BUILD.md). The portable build loads owned inputs at runtime; the original private-input build has different developer requirements. Earlier validation JSON files remain historical records of the checks available when they were written.

See the [roadmap](docs/ROADMAP.md), [preparation changes](PREPARATION-CHANGES.md), [licensing inventory](LICENSING-INVENTORY.md), [portrait provenance and rights notice](assets/PORTRAIT-NOTICE.md), and [Ringbearer notice](assets/FONT-NOTICE.md).

No additional project-wide license is granted. Existing framework and component licenses and notices remain in force, including the framework's PolyForm Noncommercial terms. Project-specific licensing and provenance choices remain documented in the licensing inventory. Portrait and font rights are separate from code licenses. Publication is not a claim of legal clearance or unrestricted open-source rights and does not override third-party rights.

HD Preset in **F10 / Escape → Video** starts with the captured expanded battlefield, HD panels, portraits and custom colors. **Advanced** exposes individual visual choices. Turning HD off/on remembers customizations and leaves resolution, fullscreen, UI size, refresh rate, audio and gameplay preferences unchanged. Existing settings retain their individual choices. The captured values are in [docs/HD-PRESET.json](docs/HD-PRESET.json). Camera smoothing and sprite caching remain separate live controls.
