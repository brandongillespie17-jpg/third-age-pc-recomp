# Windows preview setup and reviewer checklist

**[Download the Windows x64 preview ZIP](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/download/windows-preview-hd-2026-10-26/third-age-pc-windows-x64-preview-4beaa8d235b3-ringbearer.zip)** · [Release notes](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/tag/windows-preview-hd-2026-10-26)

This is the end-user package. You do not need a compiler, Python, CMake, an SDK or a dependency installer. Building from source is optional; developers can use [WINDOWS-RELEASE.md](WINDOWS-RELEASE.md) or the separate [private-input build instructions](BUILD.md).

## Setup

1. Download the ZIP and extract it into a new writable folder. Do not run directly inside the ZIP or overwrite an existing installation or save.
2. Double-click `third-age-pc.exe`. Select your own matching BIOS and ROM when prompted. Valid selections are cached for later launches; cancellation exits setup.
3. Ringbearer loads automatically from the included assets/fonts/ringbearer.ttf. Keep the complete assets/fonts folder and original READMEs. No manual font extraction or Windows font installation is required.
4. The package includes the 68 approved portraits. New game saves and preferences use the extracted installation's `saves` directory. Keep an original campaign save separate while testing.
5. Complete the checklist below and report any unchecked item as **not tested**.

Required owned inputs:

- ROM: 16,777,216 bytes; SHA-1 `0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54`.
- BIOS: 16,384 bytes; SHA-1 `300c20df6731a33952ded8c436f7f186d25d3492`.

The ZIP is 254,774,349 bytes. SHA-256:

```text
cc49d2ff3b0077e7751144c332714a35cd273a46c78158d478a25dc0766a84cb
```

## What has been verified

Windows and Linux compiled and passed the actual SDL overlay/preset/persistence regression with dummy drivers in [CI](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40405711931), built from `4beaa8d235b3e40549c675fceb1d0218431459e9`. The Windows dependency-loader and both archive content/checksum audits passed. Physical display smoothness, audio devices and full-campaign gameplay remain untested by CI; native Windows gameplay acceptance remains pending. The separately [completed publication run](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40406576676) verified both uploaded archives.

**Native Windows gameplay testing remains assigned to the reviewer.** The checks above do not establish native audio, controls, rendering, camera, save/relaunch behavior or full-campaign acceptance. Historical validation records remain unchanged and describe earlier stages.

## HD and smoothing controls

Video → HD Preset toggles the captured visuals. Advanced tweaks panels, portraits, filtering and colors; OFF/ON remembers your choices. Display → Camera Smoothing and Presentation Rate control motion independently. Gameplay → Sprite Cache exposes the rendering cache.

## Short runtime checklist

- [ ] Startup: launch from a normal Windows desktop; reach language/title/menu without a missing DLL, crash or black window.
- [ ] Audio: listen for normal speed, continuity and obvious crackling through menu, battle and combat transitions.
- [ ] Input: test mouse hover/click, keyboard actions, cancel, focus loss/re-entry and menu selection.
- [ ] Camera: test edge pan, reversals, unit focus, map edges and enemy-turn follow; note jumps, clipping or flicker.
- [ ] Battle: move, attack, cancel/re-enter, end turn and complete a battle without altered rules or stuck input.
- [ ] Saves: save in the disposable trial, close fully, relaunch and verify the same progress loads. Do not overwrite an original campaign save.
- [ ] Display: resize and test 2560×1440 when available. Record refresh rate and check game/audio pace stays normal. High presentation counts alone do not prove distinct animation frames or smoothness.
- [ ] Exit/relaunch: confirm a normal exit and repeat startup. Record whether external artwork was enabled and whether its fallback is visually correct when absent.

## Return a compact result

Report the release ZIP SHA-256, Windows version, CPU/GPU, display resolution/refresh, each checklist result, and the smallest reproduction steps for a problem. If using a source build instead, also include its commit and toolchain details. Separate build or startup success from gameplay acceptance.

Return only selected text logs after checking them for personal paths or credentials. Do not upload ROM/BIOS, generated game data, entire trial/session directories or personal saves. No public bug-report upload is required for this review.

Existing component licenses and separate portrait/font rights remain applicable; see the repository licensing inventory and the notices included in the ZIP.
