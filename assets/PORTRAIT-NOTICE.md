# Approved portrait assets: provenance and rights notice

The final assembled Windows release contains exactly the existing 68 approved
photo-based character portrait BMPs from the user's 2026-10-12 portrait set. The user explicitly asked
for these existing movie-still portraits to be included as they are in the
Windows download. The source repository retains only the small manifest and
notice; the image files are assembled locally into the final download. Their original installed BMP bytes are preserved:
no resizing, cropping, recoloring, reencoding, or new image generation was done
for this release preparation.

`PORTRAIT-MANIFEST.json` records each shipped BMP's SHA-256, byte length, roster
identity, and its approved PNG master's hash. The original local
`approved-manifest.json` had SHA-256
`2cb607c8db980e0af4b775cb8bf3b0efbd660ec8c4f0476cfaeb83d6b577f608`.
All 68 PNG masters matched that source manifest, and each BMP decoded to the
same 1254 × 1254 RGB pixels as its approved PNG. Only the existing BMPs needed by
the runtime are shipped; the PNG masters and all originals remain unchanged.

The earlier approval record describes some portraits as generated compositions
and identifies the final Sauron PNG as a later user re-upload with hash
`92b596c3599395235905da54513b3c614c997ff7c968bace0f5ef1bc79f99e28`.
That approved upload differs in file bytes from an older candidate. This package
uses the existing installed BMP corresponding to the final approved upload; it
does not claim recovery of the older candidate's bytes.

These images contain film-derived likenesses and other third-party material.
They are separate from the program's code and dependencies. The engine, SDL and
other code licenses do not license the underlying film imagery, characters,
likenesses, logos or artwork. Copyright and other rights remain with their
respective holders. No rights-holder grant, public-domain status, commercial-use
permission or independent redistribution clearance is asserted by this notice.
The user's instruction to include the files is not represented as a transfer of
third-party rights. No complete per-image film/still/creator credit chain was
available in the inspected local approval manifest; none has been invented.

This inclusion is limited to the 68 files named in PORTRAIT-MANIFEST.json. It does
not include the separate faction backgrounds, original game portrait pixels,
ROM/BIOS files, game-derived generated code, fonts, film video/music, screenshots,
personal saves or unrelated artwork. Preserve this notice and manifest with the
portrait files. The optional original font archive is handled separately under its own notice;
the runtime keeps its system-font fallback.
