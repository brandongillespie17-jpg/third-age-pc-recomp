# Clean build from local inputs

The source contains no ROM/BIOS, generated game/BIOS C++, original portrait pixels, translated graphics-routine body, font bytes or HD art. The generator reads your matching ROM/BIOS and original local Ringbearer TTF, creates all private outputs in a new directory outside the source tree, and builds the preserved campaign implementation. It does not clone, install packages, reset vendor source or launch the game.

## Required inputs and tools

- Third Age USA/Europe multilingual B3AE revision 0 ROM: 16,777,216 bytes; SHA-1 `0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54`.
- GBA BIOS: 16,384 bytes; SHA-1 `300c20df6731a33952ded8c436f7f186d25d3492`.
- Original Ringbearer `RINGM___.TTF`, supplied locally: SHA-256 `22220f17fdcec646bc8d0597421f4a589022f93753b48ce1bff6fbb23ceb477b`. The project's outline renderer expects this font. It is not bundled; no right to distribute it or embed it in a public binary is inferred. Preserve its original archive and terms separately. A ROM/BIOS-only build without this font is not claimed. No silent font substitution occurs.
- Installed Python 3, C++20 compiler, CMake >=3.20, Ninja and SDL2 development files. The modified framework/submodule sources and pinned toml++ single header plus MIT license are retained for offline configuration. Nothing downloads during this build.

## Build

From the source root, using actual paths and a new private output directory:

```
python tools/build_from_inputs.py --rom PATH_TO_OWNED_GAME.gba --bios PATH_TO_OWNED_BIOS.bin --font PATH_TO_RINGM___.TTF --output NEW_PRIVATE_TRIAL --jobs 4
```

`NEW_PRIVATE_TRIAL` must be outside the source tree and must not exist. The script validates input identities, generates both portrait sizes for all 68 characters and the bounded native graphics-decoder body directly from ROM, and converts the supplied font into a private header. It builds the recompiler from source, generates cartridge and BIOS C++ afresh, configures the runtime with private includes and builds `third-age-pc`. No private generated source is needed as input. Interrupted builds retain a `BUILD-INCOMPLETE` marker. Choose a new output directory for a clean retry.

Generation preserves every pixel and ordering of the checkpoint's 136 portrait signature tables. The generated 203-instruction graphics body matches the checkpoint byte-for-byte. The public header retains the generic transaction wrapper; generated instructions stay in a private include. Font bytes are preserved exactly. Optional HD portraits remain external through `THIRD_AGE_ASSET_DIR`, with the existing original-art fallback when unavailable.

`tools/configure_local.py` remains an advanced path for already generated private inputs. It is not needed for the clean build above. The obsolete clean-upstream Bash bootstrap and the temporary checkpoint-header migration tool are not part of the current workflow.

## Validation scope

See `BUILD-VALIDATION.json` for the actual clean Linux build result and versions. Compilation establishes a linked executable, not gameplay or fresh runtime acceptance. No game was launched during this preparation. Windows compilation, native Windows controls/audio/saves and high-refresh behavior remain pending reviewer validation.

For Windows use the existing framework LLVM/MinGW toolchain through `--cmake-arg=-DCMAKE_TOOLCHAIN_FILE=ABSOLUTE_PATH`, and provide consistent SDL2 and MinGW DLL paths. See `WINDOWS-REVIEWER.md`. Root CMake invokes the framework DLL-staging helper for dynamic MinGW builds. Its four named DLLs are not an exhaustive dependency audit. No Windows compiler was installed on the Linux inspection host.

## Launch after a successful build

The builder writes a private `launch.json` with local ROM/BIOS paths. Run:

```
python tools/launch.py NEW_PRIVATE_TRIAL
```

The launcher selects `.exe` on Windows and also checks a `Release` subdirectory. It explicitly uses the recorded campaign interpreter policy, with self-heal compilation and RAM overlays disabled, avoiding an unexpected compiler dependency during play. Saves stay in the new private trial. Existing campaign saves are not imported automatically. Never commit or upload the trial, input files, generated headers or a compiled binary containing private generated inputs.

## Presentation preferences and regression check

In the overlay (F10 or Escape), Display contains Camera Smoothing and a
60/120/144/165 Hz Presentation Rate selector. The rate is remembered when
smoothing is turned off. The expanded battlefield supplies the camera layers;
turn on Video → Expanded Battlefield to use camera interpolation. Gameplay
contains the separate experimental Sprite Cache toggle. None changes the
simulation/audio target rate. Both toggles default off for existing plain
launches, and preferences save immediately beside the selected save file.

For old launchers, THIRD_AGE_PRESENT_HZ and THIRD_AGE_DITHER_OAM_CACHE seed
preferences when their saved keys do not exist. Saved camera_smoothing,
presentation_hz and sprite_cache keys take priority, so the in-game OFF choice
continues to work on the next launch. Smooth Scaling still means pixel filtering.

Build the real SDL event/persistence regression without ROM or BIOS inputs:

```sh
cmake -S . -B build-settings -DTHIRD_AGE_PORTABLE_RELEASE=ON -DTHIRD_AGE_BUILD_PRESENTATION_TESTS=ON
cmake --build build-settings --config Release --target third-age-pc presentation-settings
ctest --test-dir build-settings -C Release -R '^presentation-settings$' --output-on-failure --no-tests=error
```

On Windows place SDL2.dll beside the test executable before running CTest; the
Windows packaging workflow does this automatically. The test uses dummy video
and audio, checks the real keyboard/mouse overlay path, live timing changes,
all supported rates, saving/reloading, legacy-flag precedence and menu layout
renders. It does not establish physical display smoothness or campaign behavior.
