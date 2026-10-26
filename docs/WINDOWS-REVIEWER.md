# Windows preview setup and reviewer checklist

**[Download the Windows x64 preview ZIP](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/download/windows-preview-2026-10-26/third-age-pc-windows-x64-preview-b8b7714f5a38.zip)** · [Release notes](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/tag/windows-preview-2026-10-26)

This is the end-user package. You do not need a compiler, Python, CMake, an SDK or a dependency installer. Building from source is optional; developers can use [WINDOWS-RELEASE.md](WINDOWS-RELEASE.md) or the separate [private-input build instructions](BUILD.md).

## Setup

1. Download the ZIP and extract it into a new writable folder. Do not run directly inside the ZIP or overwrite an existing installation or save.
2. Double-click `third-age-pc.exe`. Select your own matching BIOS and ROM when prompted. Valid selections are cached for later launches; cancellation exits setup.
3. Use the default system font. The complete original Ringbearer ZIP is optional, with its original terms and a separate notice; it is not automatically installed. Follow the packaged README if choosing that font.
4. The package includes the 68 approved portraits. New game saves and preferences use the extracted installation's `saves` directory. Keep an original campaign save separate while testing.
5. Complete the checklist below and report any unchecked item as **not tested**.

Required owned inputs:

- ROM: 16,777,216 bytes; SHA-1 `0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54`.
- BIOS: 16,384 bytes; SHA-1 `300c20df6731a33952ded8c436f7f186d25d3492`.

The ZIP is 257,616,570 bytes. SHA-256:

```text
6a8086fae52a2246c336826b2171bec11ef800b5e9a9dfe076902625c45d3356
```

## What has been verified

Build commit: `b8b7714f5a38032dcc51cf6355ea6d4189b68006`. [Windows CI run 40383530854](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40383530854) passed compilation, dependency-loader checks and ZIP auditing. A 300-frame headless Wine smoke test exited successfully and wrote a save; framebuffer, save, IWRAM and IRQ/SWI logs matched the Linux reference. Wine used a supported local font override because its fresh prefix lacked Windows system fonts.

**Native Windows gameplay testing remains assigned to the reviewer.** The checks above do not establish native audio, controls, rendering, camera, save/relaunch behavior or full-campaign acceptance. Historical validation records remain unchanged and describe earlier stages.

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
