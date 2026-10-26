# Windows reviewer setup and acceptance checklist

Windows runtime validation is assigned to the reviewer. No access to another person's computer is required. The recovered private checkpoint archive remains unchanged; this guide accompanies the separate source preparation.

## Current delivery status

This is source, not a Windows executable release. No Windows configuration, compilation or gameplay test has passed. Linux validation includes a fresh recompiler build, fresh ROM/BIOS and asset generation, and a successful runtime compile/link. Mocked launcher checks additionally verified Windows executable selection. No game was launched, and none of those checks is a Windows build or runtime test.

The clean build now generates portrait signatures, the graphics decoder, cartridge code and BIOS code from locally supplied dumps; no old generated source is required. The original local Ringbearer TTF is still required to preserve typography. Optional HD artwork remains external. No permission to publish these inputs or their generated outputs is implied.

## Setup after private inputs are available

1. Extract the preparation into a new source directory. Use new private-input and trial directories; never overlay an existing installation or save.
2. Use Python 3, CMake >=3.20, Ninja, SDL2 development files and a consistent Windows C++20 toolchain. The preserved framework includes a Windows LLVM/MinGW toolchain file. Its default locations are `C:/Program Files/LLVM` and `C:/msys64/mingw64`; adjust definitions if yours differ. No installer is supplied or run by this source preparation.
3. Follow `BUILD.md` to supply your own matching ROM, BIOS and original Ringbearer TTF. Pinned toml++ source is included. Confirm that SDL2 headers/library and runtime DLLs are all from the same target toolchain.
4. Run the configuration below from PowerShell in the source root, replacing each placeholder. These are untested Windows setup instructions derived from the preserved build files, not a successful build transcript.

```powershell
$sourceRoot = (Get-Location).Path
$toolchain = Join-Path $sourceRoot 'vendor/gbarecomp/cmake/toolchains/clang-mingw.cmake'
python tools/build_from_inputs.py --rom 'C:/ThirdAgePrivate/owned/game.gba' --bios 'C:/ThirdAgePrivate/owned/gba_bios.bin' --font 'C:/ThirdAgePrivate/owned/RINGM___.TTF' --output 'C:/ThirdAgePrivate/trial' --cmake-arg="-DCMAKE_TOOLCHAIN_FILE=$toolchain" --cmake-arg='-DGBARECOMP_STATIC_RELEASE=OFF' --cmake-arg='-DGBARECOMP_MINGW_RUNTIME_BIN=C:/msys64/mingw64/bin' --cmake-arg='-DSDL2_INCLUDE_DIR=C:/msys64/mingw64/include/SDL2' --cmake-arg='-DSDL2_LIBRARY=C:/msys64/mingw64/lib/libSDL2.dll.a'
```

The wrapper refuses an existing build directory. For a retry, choose a new directory so stale Linux or compiler cache state cannot affect the result. Capture the complete error if configuration, compilation or linking fails. Do not disable features merely to obtain an executable.

5. Check that the resulting executable's dependencies are available. The project calls the framework's MinGW helper for `SDL2.dll`, `libgcc_s_seh-1.dll`, `libstdc++-6.dll` and `libwinpthread-1.dll`; missing-DLL warnings or further imports still need resolution. No completeness claim is made for that list.
6. The builder writes `C:/ThirdAgePrivate/trial/launch.json`. If deliberately relocating the trial, adjust these local paths using forward slashes or escaped backslashes in JSON:

```json
{
  "rom": "C:/ThirdAgePrivate/owned/game.gba",
  "bios": "C:/ThirdAgePrivate/owned/gba_bios.bin"
}
```

Supported ROM SHA-1: `0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54` (16,777,216 bytes). BIOS SHA-1: `300c20df6731a33952ded8c436f7f186d25d3492` (16,384 bytes). Use your own matching dumps. Keep this JSON and the inputs private.

7. If testing privately supplied approved artwork, set `$env:THIRD_AGE_ASSET_DIR` to its external assets directory. Record whether that option was used; no HD artwork is shipped. Then launch:

```powershell
python tools/launch.py 'C:/ThirdAgePrivate/trial'
```

The launcher explicitly uses the recorded campaign interpreter policy, with self-heal compilation and RAM overlays disabled. This is the campaign baseline, not the separate opt-in click-only Amon Hen demo. The simple launcher stores its new save in the trial directory and does not import the owner's campaign progress.

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

Report preparation archive SHA-256; Windows version; CPU/GPU; display resolution/refresh; compiler, CMake and SDL versions; executable SHA-256; build success/failure; each checklist result; and the smallest reproduction steps for any problem. Label unchecked items **not tested**. Separate compiler success from gameplay acceptance.

Return only selected text logs after checking them for personal paths or credentials. Do not upload ROM/BIOS, generated game data, private headers, assets, entire trial/session directories or saves. No public bug-report upload is required for this review.
