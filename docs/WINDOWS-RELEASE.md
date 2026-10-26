# Windows x64 preview build and package

**For players and reviewers: [download the released Windows ZIP](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/download/windows-preview-2026-10-26/third-age-pc-windows-x64-preview-b8b7714f5a38.zip) or open the [release page](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/tag/windows-preview-2026-10-26).** Extract it and run `third-age-pc.exe`; no compiler, Python, CMake or dependency installer is required. Supply your own matching ROM and BIOS. See [WINDOWS-REVIEWER.md](WINDOWS-REVIEWER.md) for setup and acceptance checks.

## Current release status

The October 26, 2026 preview is published. [CI run 40383530854](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40383530854), from commit `b8b7714f5a38032dcc51cf6355ea6d4189b68006`, passed Windows compilation, dependency-loader checks and base-ZIP auditing. The final assembled release ZIP contains the executable, SDL2 runtime, 68 unchanged approved portraits and the optional complete original Ringbearer archive. A system font remains the default.

Final ZIP: 257,616,570 bytes. SHA-256:

```text
6a8086fae52a2246c336826b2171bec11ef800b5e9a9dfe076902625c45d3356
```

A bounded 300-frame Wine boot/save test passed; framebuffer, save, IWRAM and IRQ/SWI logs matched the Linux reference. That test used a supported local font override in an isolated Wine prefix. **Native Windows gameplay validation remains pending the reviewer.** Wine and CI do not prove native GUI, audio, input or complete campaign behavior. Earlier validation records are preserved as historical evidence.

## Optional developer build and packaging workflow

The remainder of this guide is for rebuilding the preview, not a prerequisite for using the download. The manual workflow builds a code-only base ZIP with `THIRD_AGE_PORTABLE_RELEASE=ON`, target `third-age-pc`, and Visual Studio output `<build>/Release/third-age-pc.exe`. A separate local assembler adds the exact approved portraits and optional complete font archive. Neither helper publishes a release automatically.

Compilation consumes no ROM, BIOS, private generated headers, game-code translations, font bytes, artwork or saves. User-owned ROM/BIOS are loaded at runtime. The original private-input source build is a separate developer path and must not be packaged by this workflow. Source and asset metadata are versioned in Git; bulky assets live in release downloads.

Existing component licenses, integration licensing/provenance decisions, and separate portrait/font rights still apply. See `LICENSING-INVENTORY.md`, `DISTRIBUTION-NOTICE.md`, `assets/PORTRAIT-NOTICE.md` and `assets/FONT-NOTICE.md`. Publishing the preview does not override third-party rights or establish blanket legal clearance.

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

The existing 68 approved photo-based portraits are supplied unchanged under
`assets/portraits`; keep that folder with the executable. No game ROM, BIOS,
bare font, personal save, film video/music or unrelated artwork is supplied.
The complete optional Ringbearer archive is supplied separately under `extras`,
with its original terms; it is not installed or loaded automatically.
Create `assets/fonts` only if choosing the optional font path. The old `WINDOWS-REVIEWER.md` private-build instructions are not the
compiler-free end-user procedure. This remains a **preview**: native Windows
startup, audio, input, battle and save/relaunch acceptance are still pending.

## Source repository, CI base and final download

The source repository contains the small portrait/font manifests and notices,
not the 320 MB of BMPs or the original font archive. The PowerShell source audit
rejects all tracked images, archives, bare fonts, ROM/BIOS, generated game data and
saves. No asset download is needed in CI. `PackageBase` packages the executable,
SDL, licenses, documentation and small asset manifests/notices only. Its README
and BUILD-INFO.json explicitly identify it as **code-only**, with zero portraits
and no font archive. Do not present that intermediate artifact as the finished
download requested by the user.

The Actions artifact is `third-age-pc-windows-x64-preview-base-<full-commit>`.
Inside it are `third-age-pc-windows-x64-preview-base-<12-character-commit>.zip` and
its `.zip.sha256`. The parent must confirm the Windows job passed and review its
exact commit/checksum before final assembly. The assembler is offline; a supplied
run URL records provenance but does not independently authenticate remote status.

The final ZIP adds exactly 68 existing BMPs (32 Good, 36 Evil), each 1254 × 1254,
totaling 320,967,480 bytes before compression. They are copied byte-for-byte:
no resizing, reencoding or image conversion. Source BMPs match the approved PNG
masters' decoded RGB pixels, and the master PNG hashes match the approval record.
Neither those PNGs nor the separate faction backgrounds are included. Originals
remain untouched. The portrait manifest SHA-256 is
`41cc3ac7361ffb0108392f30dfc2219eb12d14f233ea26868e420a466c7cb4bd`.

The final ZIP also carries `extras/Ringbearer-original.zip`, the complete,
unchanged nine-file original local package, SHA-256
`1a400cd55ece1ce992229ee167e1297c1e6373682d10ef309f8742f5b8d3d13b`.
This optional archive is never auto-extracted, installed, embedded or loaded.
The system font remains the default. No bare TTF is packaged. The original
English/German READMEs and all companion files remain inside the archive; its
separate notice paraphrases the author's free complete-archive distribution,
private-use-only, noncommercial and no-modification restrictions. This is not a
claim of permission to use the font in a publicly distributed game. The font
manifest SHA-256 is
`8473afaf9fcddafd18fa25191645558c81647eba80f8fac82415ad30b06d318f`.

## Assemble the single final release ZIP

Run the Python 3.11+ maintainer helper after a successful reviewed Windows job.
Python is used only for packaging, not on the player's computer. Supply the inner
base ZIP from the Actions artifact and the checksum/commit verified from that run:

```powershell
python tools/assemble_windows_release.py `
  --base-zip 'C:/ReleaseInputs/third-age-pc-windows-x64-preview-base-<commit12>.zip' `
  --expected-base-sha256 '<reviewed-base-zip-sha256>' `
  --expected-commit '<reviewed-full-commit-sha>' `
  --ci-run-url 'https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/<run-id>' `
  --portraits-dir 'C:/ReleaseInputs/approved-portraits' `
  --font-archive 'C:/ReleaseInputs/ringbearer.zip' `
  --output-dir 'C:/ReleaseOutput/new-preview'
```

The helper accepts only the exact base entry list; rejects unsafe, duplicate or
symlink ZIP paths; checks every base file's checksum, the expected commit, audit
metadata, x64 PE headers and exact official SDL DLL; and verifies every selected
BMP and the entire font archive against fixed manifests/hashes. Extra files in
the local portrait directory are never read or copied. No recursion, network
request, executable launch or asset transformation occurs.

The output directory must be new. Output is one
`third-age-pc-windows-x64-preview-<commit12>.zip` plus its companion `.zip.sha256`.
The final archive includes a regenerated SHA256SUMS.txt, unchanged CI executable
and DLL bytes, final BUILD-INFO.json, and original BASE-BUILD-INFO.json. It is
read back and every entry hash checked before the ZIP loses its `.partial`
filename. Native Windows GUI/gameplay acceptance remains pending after assembly.

## Package contents and audits

`tools/package_windows.ps1` uses explicit file copies into a new directory. The
only executable files allowed are `third-age-pc.exe` and the exact vendor x64
`SDL2.dll`. Other files are README.txt, BUILD-INFO.json, SHA256SUMS.txt, required
license/attribution notices, and the two exact MPL-covered `bios_hle.cpp/.h` source
files with their license. The base contains only the small portrait/font manifests and notices. The final
assembler adds the 68 exact BMPs, original optional font archive and base metadata. No recursive build-folder or asset-folder copy
is used. MPL source is
included directly and the README points to the full immutable source revision.

The source preflight rejects dirty/untracked inputs, symlinks, binary data,
image/font/archive/dump/save extensions and known private/generated input paths.
It allows only the four named asset notice/manifest text files, verifying the two
manifest pins. It permits the tracked empty BIOS dispatch stub. CI runs this
before fetching SDL or building, then repeats it before packaging. The final
assembler provides the separate exact-byte image/archive exceptions. Static file
checks cannot prove provenance or detect every encoded payload in source text.

For each shipped PE, `dumpbin /headers` must identify x64; `/dependents` must
resolve to the other bundled DLL or an explicit Windows-system/API-set allowlist.
Unexpected, debug or compiler-runtime DLLs fail closed. The loader probe uses
only the staged directory and Windows system PATH. It runs
`--ci-package-loader-probe` and expects exit 1 plus `unknown argument` before
input selection or GUI startup. `--help` is deliberately not used because the
current runtime continues startup for that argument. The probe has a 15-second
timeout and supplies no private inputs. Dynamically loaded optional driver DLLs
and actual graphics/audio initialization still need desktop testing.

Both ZIP stages have sorted entries and fixed timestamps, an exact entry allowlist,
and a read-back SHA-256 comparison of every entry. SHA256SUMS.txt covers all other
packaged files; a sibling `.zip.sha256` hashes the whole ZIP. CI upload is limited to
its base ZIP and checksum, never build directories, logs, dumps, caches or private
inputs. Final release publication is a separate parent-coordinated action.
Artifacts expire after 14 days and are preview artifacts, not GitHub Releases.

This is a repeatable recipe with pinned source/dependency/action revisions, not a
claim of bit-for-bit build reproducibility: hosted image/tool versions change.
BUILD-INFO.json records runner image, CMake/compiler version, commit, dependency
hash and import audit. Each ZIP stage is deterministic only for identical input bytes and tool versions.

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
