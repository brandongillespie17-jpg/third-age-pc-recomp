# Included material and publication decisions

This is an inventory of notices present in the inspected source. The owner has authorized public source review at [brandongillespie17-jpg/third-age-pc-recomp](https://github.com/brandongillespie17-jpg/third-age-pc-recomp), and the staged content audit is complete.

No additional project-wide license is granted. Existing framework and component licenses and notices remain in force. Project-specific licensing and provenance choices remain documented below. This publication is a source-review snapshot, not a claim of legal clearance or unrestricted open-source rights, and does not override third-party rights.

| Included material | Origin / recorded terms | Evidence and remaining decision |
|---|---|---|
| gbarecomp source and upstream documentation | Matthew Stanley / mstan gbarecomp; PolyForm Noncommercial 1.0.0 with the retained clarification | `vendor/gbarecomp/LICENSE`. Preserve notices and noncommercial terms; do not relabel the entire repository MIT. |
| ARM decode/IR/interpreter/code emission core | arm-recomp-core, originally extracted from mstan/ndsrecomp; MIT, copyright 2026 Matthew Stanley | `external/arm-recomp-core/LICENSE` and `PROVENANCE.md`. Described there as project-authored clean-room code. |
| Network and rollback source | recomp-net contributors and retcomm-rbengine contributors; MIT | Individual `external/recomp-net/LICENSE` and `external/rbengine/LICENSE`. No network service credentials or remote accounts are bundled. |
| toml++ v3.4.0 single header | Mark Gillard; MIT | `third_party/tomlpp/LICENSE` and `toml.hpp`. Source came from the retained pinned dependency checkout; header SHA-256 is recorded in `DEPENDENCIES.json`. This dependency is source, not game data. |
| `bios_hle.cpp` and `bios_hle.h` | mGBA-derived files; MPL-2.0, separately licensed from the framework | `THIRD_PARTY_ATTRIBUTION.md`, file notices and `third_party/MPL-2.0.txt`. Preserve the modified covered source and notices with any applicable distribution. No libmgba binary is bundled. |
| Other credited framework ports | MIT OR Apache-2.0 per framework attribution, including JRickey/gba-recomp-derived portions | Preserve file headers and `THIRD_PARTY_ATTRIBUTION.md`; upstream's recorded attribution is evidence, not this handoff independently proving the full provenance chain. |
| Third Age integration, PC adapters, launch/build tools and project docs | Recovered local project modifications plus preparation changes; no project-level license was found | Owner must select/confirm terms for contributions they can license, compatible with upstream obligations. Exact-ROM adapters and newly recovered generator/exporter logic need attribution/provenance review; upstream licensing alone does not establish rights to every local modification. |
| Generic portrait decoder and graphics code generator | Source algorithms implemented for local generation; output derives from the user's own ROM | No portrait pixels or translated graphics instruction sequence is included. Both are generated privately. Contribution licensing and provenance remain an explicit owner decision; no additional license is granted here. |
| SDL2 and compiler runtimes | Official SDL2 2.32.10 x64 DLL (zlib), pinned vendor archive; MSVC static runtime linkage | The Windows base includes the exact SDL DLL and retained notices; unexpected DLL imports fail packaging. Source control contains no binaries. Applicable compiler-runtime distribution terms remain in force. See docs/WINDOWS-RELEASE.md. |
| Approved movie/photo-based portraits | Existing user-approved set of 68 BMPs, copied unchanged only into the final Windows ZIP | User explicitly requested inclusion. assets/PORTRAIT-MANIFEST.json pins the files; assets/PORTRAIT-NOTICE.md records provenance limits. Code licenses do not grant rights to film imagery, and no independent rights-holder clearance is claimed. |
| Optional original Ringbearer archive | Ringbearer Medium v1.00, © Pete Klassen 2002; complete unchanged nine-file original local ZIP | Final ZIP only: extras/Ringbearer-original.zip. Original README permits free complete-archive distribution, limits use to private use, and prohibits commercial use/modification. No bare TTF, installation, embedding or automatic loading; system-font default. See assets/FONT-MANIFEST.json and assets/FONT-NOTICE.md. |

All shortened paths above are relative to `vendor/gbarecomp/` unless explicitly named as project files.

## Excluded assets and generated material

ROM and BIOS dumps; generated cartridge/BIOS C++; translated graphics-decoder bodies; original game portrait pixels; bare/embedded font bytes; screenshots; saves; game states; private headers; trial/configuration paths; build products and caches remain excluded from source control and the public package. The repository also excludes image and font-archive binaries: only manifests/notices are versioned. Hashes and generator interfaces do not convey rights.

The final Windows preview is assembled separately from a successful audited code-only CI base. Its narrow additional assets are the exact 68 approved portrait BMPs and optional complete original Ringbearer ZIP. No unrelated artwork, faction-background files, film video/music or original game-portrait reference images are included. The author's font README and all companion files stay inside the unchanged archive. Free distribution of the complete archive is not represented as permission for public-game use or font embedding. The user's portrait instruction does not transfer third-party rights. This handoff does not independently determine rights in underlying game or film materials.

## Exact unresolved public-sharing decisions

1. Confirm who can license the local Third Age integration and documentation, and choose explicit contribution terms consistent with retained vendor licenses. Do not replace upstream licenses.
2. Review ROM-specific hand-written adapters and the recovered extraction/generation logic for authorship/provenance. A no-blob audit is not a copyright clearance.
3. Keep the repository limited to source and small notices/manifests. Do not upload private inputs, generated outputs, images, font archives or locally compiled executables to Git history. The authorized final Windows preview is a separate reviewed download assembled from the CI base and the exact asset exceptions described above.
4. The public repository exists and GitHub installation access has been verified. The owner has authorized publication of the audited source-review snapshot. That authorization does not choose contribution license terms, resolve provenance questions, or grant rights to excluded third-party material.
