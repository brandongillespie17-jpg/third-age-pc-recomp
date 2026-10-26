# Linux x86_64 preview

[Download tar.gz](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/download/linux-preview-hd-2026-10-26/third-age-pc-linux-x86_64-preview-4beaa8d235b3.tar.gz) · [Release notes](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/tag/linux-preview-hd-2026-10-26)

Built on Ubuntu 24.04. Requires glibc 2.39+, SDL2 and a system TrueType font. The C++ runtime is statically linked. On Ubuntu, install runtime dependencies with `sudo apt install libsdl2-2.0-0 fonts-dejavu-core`.

Extract the whole archive into a new writable folder. First launch:

```sh
./play.sh --bios "/absolute/path/gba_bios.bin" --rom "/absolute/path/game.gba"
```

Later launches: `./play.sh`. Validated BIOS/ROM paths are cached. Linux does not currently show Windows file-selection dialogs. No compiler, Python or Wine is needed.

The supported BIOS SHA-1 is `300c20df6731a33952ded8c436f7f186d25d3492` (16,384 bytes). ROM SHA-1 is `0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54` (16,777,216 bytes). Supply your own files.

New saves go to `saves/third-age.sav`. To resume a campaign, back up your existing save and pass `--save "/absolute/path/save.sav"`. Settings live beside the selected save. Keep assets with the executable.

F10/Escape → Video → HD Preset toggles captured visuals; Advanced adjusts individual choices and colors. OFF/ON preserves your tweaks. Display → Camera Smoothing and Presentation Rate control camera presentation separately; Gameplay → Sprite Cache controls its renderer cache. Monitor settings are independent of HD.

Archive: 254,611,003 bytes. SHA-256: `a2feacb9584987e8f1d98be7bfe75f1939712b2e66b8e0cd31c13d1eb2aed7a8`.

Windows and Linux compiled and passed the actual SDL overlay/preset/persistence regression with dummy drivers in [CI](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40405711931), built from `4beaa8d235b3e40549c675fceb1d0218431459e9`. The Windows dependency-loader and both archive content/checksum audits passed. Physical display smoothness, audio devices and full-campaign gameplay remain untested by CI; native Windows gameplay acceptance remains pending. The separately [completed publication run](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40406576676) verified both uploaded archives.

For a source build, use CMake with `THIRD_AGE_PORTABLE_RELEASE=ON`, `THIRD_AGE_BUILD_PRESENTATION_TESTS=ON`, `GBARECOMP_SELFHEAL_RECOMPILE_DEFAULT=OFF`, and `GBARECOMP_ENABLE_MODS=OFF`; the exact commands are in `.github/workflows/publish-desktop.yml`. No private game inputs are required to compile the portable runtime.
