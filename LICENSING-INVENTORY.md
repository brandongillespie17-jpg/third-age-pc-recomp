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
| SDL2 and compiler runtimes | Required external toolchain/runtime dependencies; none distributed as binaries here | Check their actual versions and notices before any future executable/DLL release. The source-only archive does not settle binary redistribution obligations. |

All shortened paths above are relative to `vendor/gbarecomp/` unless explicitly named as project files.

## Excluded assets and generated material

ROM and BIOS dumps; generated cartridge/BIOS C++; the 203-instruction graphics-decoder translation; original portrait signature pixels; Ringbearer font bytes; approved HD/movie-derived artwork; screenshots; saves; game states; binaries; private headers; trial/configuration paths; build products and caches are excluded. Their hashes and generator interfaces do not convey distribution rights.

The local Ringbearer notice says Pete Klassen, 2002, all rights reserved; free distribution only with the complete original archive, private use only, and no commercial use or modification. The font and archive are not included. A successful private build does not authorize sharing an executable embedding the font. Optional private artwork approval is not public redistribution permission. This handoff does not independently determine rights in underlying game or film materials.

## Exact unresolved public-sharing decisions

1. Confirm who can license the local Third Age integration and documentation, and choose explicit contribution terms consistent with retained vendor licenses. Do not replace upstream licenses.
2. Review ROM-specific hand-written adapters and the recovered extraction/generation logic for authorship/provenance. A no-blob audit is not a copyright clearance.
3. Keep the repository source-only. Do not upload private inputs, generated outputs or the locally compiled executable. Any later binary release needs its own game/font/runtime/art license review.
4. The public repository exists and GitHub installation access has been verified. The owner has authorized publication of the audited source-review snapshot. That authorization does not choose contribution license terms, resolve provenance questions, or grant rights to excluded third-party material.
