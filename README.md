# The Third Age GBA PC recomp

**[Windows x64 ZIP](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/download/windows-preview-hd-2026-10-26/third-age-pc-windows-x64-preview-4beaa8d235b3-ringbearer.zip)** · [Windows release notes](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/tag/windows-preview-hd-2026-10-26)

**[Linux x86_64 tar.gz](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/download/linux-preview-hd-2026-10-26/third-age-pc-linux-x86_64-preview-4beaa8d235b3.tar.gz)** · [Linux release notes](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/releases/tag/linux-preview-hd-2026-10-26)

## Install and run

Download the Windows ZIP or Linux tar.gz above from the release's **Assets** list. The GitHub **Source code** archives are for developers, not ready-to-play downloads. No compiler, Python or Wine is needed for the native releases.

Supply your own matching **GBA** game ROM and BIOS; neither is included. This project is for the Game Boy Advance edition of The Third Age.

| Input | Size | Required SHA-1 |
| --- | ---: | --- |
| Game ROM | 16,777,216 bytes | `0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54` |
| GBA BIOS | 16,384 bytes | `300c20df6731a33952ded8c436f7f186d25d3492` |

### Windows x64

1. Extract the **entire ZIP** into a new writable folder, such as a folder under Documents. Do not launch from inside the ZIP.
2. Keep `SDL2.dll`, `assets`, and the other included folders beside `third-age-pc.exe`.
3. Double-click `third-age-pc.exe`, then select your BIOS and ROM in the file pickers. Incorrect inputs are rejected.
4. Later launches remember the validated file paths. Keep the ROM and BIOS at those locations, or select them again if you move them.
5. Ringbearer loads automatically from the included `assets/fonts` folder. No font installation is needed.

### Linux x86_64

Use Ubuntu 24.04 or a compatible newer distribution with glibc 2.39+, SDL2 and a TrueType font. On Ubuntu, install the runtime dependencies:

```sh
sudo apt install libsdl2-2.0-0 fonts-dejavu-core
```

Extract the **entire tar.gz** into a new writable folder. Open a terminal in that extracted folder and run:

```sh
./play.sh --bios "/absolute/path/gba_bios.bin" --rom "/absolute/path/game.gba"
```

For later launches, run `./play.sh`. Linux remembers validated paths but does not have the Windows file-selection dialogs. Keep the assets with the executable. If the launcher lost its executable permission during extraction, run `chmod +x play.sh third-age-pc`.

## Mouse and keyboard controls

These are the shipped defaults. An existing `keybinds.ini` beside the executable can override the game keys.

| Keyboard | Action |
| --- | --- |
| Arrow keys | Move the game cursor / navigate game menus (GBA D-pad) |
| **X** | Confirm / select (GBA A) |
| **Z** | Cancel / back (GBA B) |
| **Enter** | Start / original game menu (GBA Start) |
| **Right Shift** | GBA Select |
| **C / V** | GBA L / R shoulder actions, depending on the current screen |
| **F10 or Escape** | Open / close the PC settings overlay; the game pauses while it is open |
| **Space** | Skip the intro when available; skip a fight when Gameplay → Press Space to Skip is enabled |

**Escape opens PC settings. Use Z or right-click to cancel a game action.** The default movement keys are arrows; WASD is not mapped by default.

| Mouse | Action |
| --- | --- |
| Move pointer | Hover over supported menu entries and battlefield tiles |
| Left-click | Select / confirm; choose a unit or a valid movement destination |
| Repeat-click / double-click a unit | Request that unit's action menu after selection |
| Right-click | Cancel / back (GBA B) |
| Middle-click | GBA Select |
| Wheel up / down | Navigate supported lists; cycle attack targets or deployment choices in those modes |
| Move pointer to a battlefield edge | Pan the camera with Controls → Edge-Only Camera enabled |

Mouse actions follow the current game mode. During attack targeting or deployment, use the wheel to cycle choices and left-click to confirm. If a screen does not support pointing at individual entries, use the keyboard. Mouse controls and edge scrolling can be adjusted under **F10 / Escape → Controls**, including edge speed and scrolling beyond the window while it has focus.

In the **PC settings overlay**, click tabs and settings with the mouse, or use **Tab / Shift+Tab** to change tabs, **Up / Down** to select a row, and **Left / Right** to change its value. **Enter / Space** activates the selected row. Escape backs out of the advanced/color screens before closing the overlay. Select **Exit Game** to quit.

## HD, camera smoothing and display

**F10 / Escape → Video → HD Preset** toggles the captured expanded battlefield, HD panels, portraits and custom colors. **Advanced** exposes individual visual choices. Turning HD off/on remembers customizations and leaves resolution, fullscreen, UI size, refresh rate, audio, controls and gameplay preferences unchanged. Existing settings retain their individual choices. Captured values: [docs/HD-PRESET.json](docs/HD-PRESET.json).

**Display → Camera Smoothing** applies immediately and persists. **Presentation Rate** offers 60/120/144/165 Hz. Camera interpolation uses the expanded battlefield. **Gameplay → Sprite Cache** independently controls the experimental renderer cache. The simulation keeps its normal speed.

Camera Smoothing works with **Video → HD Preset → ON** and **Video → Advanced → Expanded Battlefield → ON**. Set **Display → Camera Smoothing → ON** and choose a **Presentation Rate** of 60, 120, 144 or 165 Hz to suit your monitor. **Smooth Scaling** is a separate pixel-filtering setting. The HD toggle does not change camera smoothing or your monitor settings.

To change resolution, window mode or overlay size, open **Display**, choose the settings, select **Apply Display Settings**, then **Keep**. The preview reverts after 15 seconds unless kept; Escape reverts immediately. Most other preferences save automatically.

## Save, quit and update

Use the game's own save command, then **F10 / Escape → Exit Game**. Closing the overlay or changing a preference does not create a campaign save.

New campaigns use `saves/third-age.sav` in the extracted installation. Host preferences are stored in `third-age-settings.ini` beside the selected save. To move to a newer release, close the game, back up your old `saves` folder, extract the new release into a separate folder, and copy your save and preferences into its `saves` folder before launching.

To use another save without copying it, back it up and pass its path on each launch. In Windows PowerShell from the extracted folder:

```powershell
.\third-age-pc.exe --save "C:\path\to\campaign.sav"
```

On Linux:

```sh
./play.sh --save "/absolute/path/campaign.sav"
```

That selected file is the active writable save. It is not an automatic disposable copy. Supply `--bios` and `--rom` as well on a first Linux launch.

## Troubleshooting

- **Missing SDL2.dll on Windows:** extract the complete ZIP and keep the DLL beside the executable.
- **ROM or BIOS rejected:** check the GBA file sizes and SHA-1 hashes listed above.
- **Linux reports missing SDL2 or GLIBC:** install the listed runtime dependencies and use a compatible distribution with glibc 2.39 or later.
- **Settings do not save:** move the extracted installation to a folder your account can write to; check that the selected save's directory is writable too.
- **Camera still feels choppy:** check Camera Smoothing, Presentation Rate and Expanded Battlefield as described above. Raising presentation rate does not increase the game's simulation speed.
- **Windows still uses the old font:** use the `-ringbearer.zip` download above and keep its complete `assets/fonts` folder beside the executable.

For more detail, see [Windows setup](docs/WINDOWS-REVIEWER.md) or [Linux setup](docs/LINUX-RELEASE.md).

## Project and release information

Both releases include the same 68 approved portraits and original Ringbearer archive, with notices. The corrected Windows package loads Ringbearer automatically from assets/fonts/ringbearer.ttf and includes all original font files. The existing Linux package keeps its system-font default and optional font archive. No ROM, BIOS or personal save is included. Each release has an archive checksum and per-file hashes.

Windows and Linux compiled and passed the actual SDL overlay/preset/persistence regression with dummy drivers in [CI](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40405711931), built from `4beaa8d235b3e40549c675fceb1d0218431459e9`. The Windows dependency-loader and both archive content/checksum audits passed. Physical display smoothness, audio devices and full-campaign gameplay remain untested by CI; native Windows gameplay acceptance remains pending. The separately [completed publication run](https://github.com/brandongillespie17-jpg/third-age-pc-recomp/actions/runs/40406576676) verified both uploaded archives.

This preserves the hybrid GBA runtime and PC presentation/control enhancements from the October 15 camera-weather checkpoint. It is not a complete native/static port. The separate limited PC-native Amon Hen prototype is not merged.

Source builds are optional: see the [portable Windows build and package guide](docs/WINDOWS-RELEASE.md) or the [private-input source build instructions](docs/BUILD.md). The portable build loads owned inputs at runtime; the original private-input build has different developer requirements. Earlier validation JSON files remain historical records of the checks available when they were written.

See the [roadmap](docs/ROADMAP.md), [preparation changes](PREPARATION-CHANGES.md), [licensing inventory](LICENSING-INVENTORY.md), [portrait provenance and rights notice](assets/PORTRAIT-NOTICE.md), and [Ringbearer notice](assets/FONT-NOTICE.md).

No additional project-wide license is granted. Existing framework and component licenses and notices remain in force, including the framework's PolyForm Noncommercial terms. Project-specific licensing and provenance choices remain documented in the licensing inventory. Portrait and font rights are separate from code licenses. Publication is not a claim of legal clearance or unrestricted open-source rights and does not override third-party rights.
