# Windows x64 preview build and package

This workflow prepares a compiler-free **preview** of the public runtime. It does
not publish a GitHub Release. The parent/runtime owner confirmed
`THIRD_AGE_PORTABLE_RELEASE=ON`, target `third-age-pc`, and Visual Studio output
`<build>/Release/third-age-pc.exe`. The public build must not consume ROM, BIOS,
private headers, Python-generated game code, Ringbearer bytes, artwork or saves.
The original private-input build is a separate path and must never be packaged by
this workflow.

## Status and remaining gates

The workflow has **not been dispatched**. Windows compilation, the Windows loader
probe and actual ZIP generation have not run in this Linux preparation session.
The runtime owner reports that Linux regressions passed. Those results, local
source/static checks and inspection of the official SDL Windows DLL are not
Windows build or gameplay results.

The parent has authorized the initial build and audited ZIP artifact with a
read-only token, then will review the resulting binary package before publishing
a preview release. There is no automated release publication. Existing
`LICENSING-INVENTORY.md` and `DISTRIBUTION-NOTICE.md` still describe a source-only
snapshot and unresolved integration contribution/provenance decisions; retain
those notices and resolve applicable binary release terms in the parent's final
review. A no-blob scan cannot settle those questions. Excluded private assets and
unreviewed third-party binaries must not be added to get past a failed check.

The runtime owner has confirmed the startup behavior below. Before the first run,
the parent should review and commit the integrated runtime and packaging changes. Source-review baseline before this work:
`eaeead77171cc6e843fc57a99715f7221b057e09`. The build uses the **dispatched commit**,
not that obsolete baseline. The checkout SHA is recorded in the ZIP and filename.
Do not dispatch this preparation before that coordination is complete.

## Toolchain and pinned dependency

The job uses GitHub-hosted `windows-2022`, the preinstalled Visual Studio 2022 x64
MSVC tools and CMake. Nothing is installed on the owner's computer. No package
manager, installer, private runner, repository secret, restored build cache or
personal input directory is used. The workflow has read-only repository access;
checkout credentials are not persisted. There are no push, pull-request or tag
triggers, and no release/publication step.

SDL is pinned to the official upstream VC development archive, which also
contains the redistributable x64 runtime DLL:

- Version: `2.32.10`.
- Vendor URL: <https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip>.
- SHA-256: `af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e`.
- Size observed: `7172966` bytes.
- The download was independently hashed during preparation and matched the
  `digest` field of the official [GitHub release API](https://api.github.com/repos/libsdl-org/SDL/releases/tags/release-2.32.10).
- Build headers: `SDL2-2.32.10/include`; link library: `lib/x64/SDL2.lib`;
  shipped DLL: `lib/x64/SDL2.dll`. No x86 binaries, headers or development libraries
  are shipped. The DLL and its notice bytes are rechecked against the pinned ZIP.

SDL's [installation guidance](https://wiki.libsdl.org/SDL2/Installation) and
[license](https://github.com/libsdl-org/SDL/blob/release-2.32.10/LICENSE.txt) cover
its dynamic library distribution. Its unmodified LICENSE.txt and README-SDL.txt
are retained. The observed official x64 DLL imports only these Windows libraries:
SETUPAPI, WINMM, IMM32, VERSION, KERNEL32, USER32, GDI32, ADVAPI32, SHELL32, ole32,
and OLEAUT32. CI verifies the actual shipped files again with MSVC `dumpbin`.

The executable uses `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` (`/MT`) so it does
not require a Visual C++ redistributable installer. The MinGW-specific
`GBARECOMP_STATIC_RELEASE` is **OFF**, leaving SDL dynamically linked. No
VCRUNTIME, MSVCP, MinGW or compiler DLL is copied from the runner. If one appears
in the import table, packaging fails; review and correct linkage or explicitly
review any required runtime's terms and notices before changing the allowlist.
This does not waive the applicable MSVC redistributable-code terms for linked
runtime code; the parent distribution review must cover those too.

Actions are pinned to verified official commits: checkout v4.2.2
`11bd71901bbe5b1630ceea73d27597364c9af683`, upload-artifact v4.6.2
`ea165f8d65b6e75b540449e92b4886f43607fa02`. See the official
[runner inventory](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md),
[workflow syntax](https://docs.github.com/en/actions/writing-workflows/workflow-syntax-for-github-actions)
and [artifact action](https://github.com/actions/upload-artifact).

## Build contract

The checked-in workflow is the authoritative invocation. Core configuration:

```powershell
cmake -S . -B build-windows -G 'Visual Studio 17 2022' -A x64 `
  -DTHIRD_AGE_PORTABLE_RELEASE=ON -DTHIRD_AGE_ALLOW_HEADLESS=OFF `
  -DGBARECOMP_STATIC_RELEASE=OFF -DGBARECOMP_COMPILER_CACHE=OFF `
  -DGBARECOMP_SELFHEAL_RECOMPILE_DEFAULT=OFF -DGBARECOMP_ENABLE_MODS=OFF `
  -DGBARECOMP_BUILD_ORACLE=OFF -DGBA_COSIM=OFF `
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON `
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
  "-DSDL2_INCLUDE_DIR=$SdlRoot/include" "-DSDL2_LIBRARY=$SdlRoot/lib/x64/SDL2.lib"
cmake --build build-windows --config Release --target third-age-pc --parallel 2
```

The public runtime must accept the user's inputs at runtime and never invoke a
compiler/self-heal compilation. It must not link old translated game/BIOS code or
embedded private font/portrait data. Its no-asset handling must be explicit and
usable. CMake and runtime implementation are owned by the runtime engineer; this
packaging work does not change them. Any unused-option CMake warning affecting
this contract requires review, not an assumption that the flag took effect.

The package script checks the public BOOL option, interactive SDL mode, static
MSVC CRT setting and disabled self-heal default in CMakeCache.txt. It requires an
actual SDL2 import and a clean source checkout. The parent must review the source
implementation as well; a cache entry by itself cannot prove safe behavior.

## User startup

1. Extract the complete ZIP to a **new writable folder** and run
   `third-age-pc.exe`. No compiler, Python, SDK or dependency installer is needed.
2. Use the Windows file pickers to select your own matching BIOS and game ROM.
   Both are strictly checked; an incorrect size/hash is rejected. Supported inputs:

   | Input | Size | SHA-1 |
   | --- | ---: | --- |
   | GBA BIOS | 16,384 bytes | `300c20df6731a33952ded8c436f7f186d25d3492` |
   | Third Age GBA ROM | 16,777,216 bytes | `0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54` |

   The selected paths are remembered in `bios.cfg` and `rom.cfg` beside the
   executable. Keep these files and your inputs private; they are not packaged.
3. The interface uses an installed system font by default. No font installation
   or Ringbearer file is required. To use your own permitted font, optionally put
   it at `assets/fonts/ringbearer.ttf` beside the executable, or set `THIRD_AGE_FONT`
   to a local TrueType font path before launching. An invalid explicit
   `THIRD_AGE_FONT` selection produces an error rather than silently changing it.
4. New saves default to `saves/third-age.sav` beside the executable. Do not merge
   this preview into an existing campaign/save directory or copy over an original
   save. Use a disposable new save for the first review.

No game, BIOS, Ringbearer font, portraits, HD artwork or personal save is supplied.
The ZIP needs no assets folder; create `assets/fonts` only if choosing that optional
font path. The old `WINDOWS-REVIEWER.md` private-build instructions are not the
compiler-free end-user procedure. This remains a **preview**: native Windows
startup, audio, input, battle and save/relaunch acceptance are still pending.

## Package contents and audits

`tools/package_windows.ps1` uses explicit file copies into a new directory. The
only executable files allowed are `third-age-pc.exe` and the exact vendor x64
`SDL2.dll`. Other files are README.txt, BUILD-INFO.json, SHA256SUMS.txt, required
license/attribution notices, and the two exact MPL-covered `bios_hle.cpp/.h` source
files with their license. No recursive build-folder copy is used. MPL source is
included directly and the README points to the full immutable source revision.

The source preflight rejects dirty/untracked inputs, symlinks, NUL/binary data,
asset/dump/save extensions and known private/generated input paths. It permits
the tracked empty BIOS dispatch stub. CI runs this before fetching SDL or
building, then repeats it before packaging. This is a static input check, not a
proof that text contains no encoded assets or that all code has clear provenance.

For each shipped PE, `dumpbin /headers` must identify x64; `/dependents` must
resolve to the other bundled DLL or an explicit Windows-system/API-set allowlist.
Unexpected, debug or compiler-runtime DLLs fail closed. The loader probe uses
only the staged directory and Windows system PATH. It runs
`--ci-package-loader-probe` and expects exit 1 plus `unknown argument` before
input selection or GUI startup. `--help` is deliberately not used because the
current runtime continues startup for that argument. The probe has a 15-second
timeout and supplies no private inputs. Dynamically loaded optional driver DLLs
and actual graphics/audio initialization still need desktop testing.

The final ZIP has sorted entries and fixed timestamps, an exact entry allowlist,
and a read-back SHA-256 comparison of every entry. SHA256SUMS.txt covers all other
packaged files; a sibling `.zip.sha256` hashes the whole ZIP. Upload is limited to
those two files, never build directories, logs, dumps, source caches or inputs.
Artifacts expire after 14 days and are preview artifacts, not GitHub Releases.

This is a repeatable recipe with pinned source/dependency/action revisions, not a
claim of bit-for-bit build reproducibility: hosted image/tool versions change.
BUILD-INFO.json records runner image, CMake/compiler version, commit, dependency
hash and import audit. The ZIP is deterministic only for identical input bytes.

## Acceptance and failures

A passed job proves compilation, file selection, static import checks and a
minimal Windows process/CLI load. It does **not** prove title boot, graphics,
audio, input, camera, battle correctness, saves, performance or compiler-free
runtime behavior under all paths. Keep the release labeled **preview** until a
native Windows desktop reviewer completes those checks using their own permitted
inputs and a disposable new save directory. CI is not that gameplay reviewer.

For a failed build, retain only scrubbed diagnostic text; never upload private
inputs or a whole local trial directory. For a missing DLL, inspect the named
import and its architecture/terms instead of copying System32 or runner folders.
For failed asset/input handling, return the issue to the runtime owner. For unresolved
release terms, resolve the specific inventory entries with the parent before
publishing the preview release. Never
bypass the audit, replace an unknown dependency with a random download, or use a
previous Linux/private build to manufacture a passing artifact.
