#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('AuditSource', 'PackageBase')][string]$Mode,
    [Parameter(Mandatory)][string]$SourceRoot,
    [string]$BuildRoot,
    [string]$Executable,
    [string]$SdlRoot,
    [string]$SdlArchive,
    [string]$PublicRuntimeCMakeOption,
    [string]$OutputDirectory,
    [string]$Commit
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$FontManifestSha256 = '8473afaf9fcddafd18fa25191645558c81647eba80f8fac82415ad30b06d318f'
$PortraitManifestSha256 = '41cc3ac7361ffb0108392f30dfc2219eb12d14f233ea26868e420a466c7cb4bd'
$SdlSha256 = 'af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e'
$SdlUrl = 'https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip'
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path

function Get-Sha256([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Assert-RegularFile([string]$Path) {
    $item = Get-Item -LiteralPath $Path
    if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Not a regular file: $Path" }
}
function Get-ApprovedPortraits {
    $path = Join-Path $SourceRoot 'assets/PORTRAIT-MANIFEST.json'
    Assert-RegularFile $path
    if ((Get-Sha256 $path) -ne $PortraitManifestSha256) { throw 'Approved portrait manifest hash changed; explicit review is required.' }
    $manifest = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 'third-age-approved-portrait-package-v1' -or $manifest.count -ne 68 -or @($manifest.files).Count -ne 68) {
        throw 'Expected the exact approved 68-portrait manifest.'
    }
    $map = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    foreach ($entry in $manifest.files) {
        if ($entry.file -cnotmatch '^assets/portraits/[a-z0-9-]+\.bmp$' -or $entry.sha256 -cnotmatch '^[0-9a-f]{64}$' -or $map.ContainsKey($entry.file)) {
            throw 'Invalid or duplicate approved portrait entry.'
        }
        $map.Add($entry.file, $entry)
    }
    return ,$map
}
function Assert-SourceInputs {
    $status = & git -C $SourceRoot status --porcelain --untracked-files=all
    if ($LASTEXITCODE -ne 0 -or $status) { throw 'Source must be a clean Git checkout with no untracked inputs.' }
    $files = @(& git -C $SourceRoot -c core.quotepath=false ls-files)
    if ($LASTEXITCODE -ne 0 -or !$files.Count) { throw 'Cannot inventory tracked source.' }
    $null = Get-ApprovedPortraits # Verify the small, pinned manifest; no BMPs are required by CI.
    if ((Get-Sha256 (Join-Path $SourceRoot 'assets/FONT-MANIFEST.json')) -ne $FontManifestSha256) { throw 'Optional font manifest hash changed.' }
    $assetText = @('assets/PORTRAIT-MANIFEST.json', 'assets/PORTRAIT-NOTICE.md', 'assets/FONT-MANIFEST.json', 'assets/FONT-NOTICE.md')
    foreach ($relative in $files) {
        $path = Join-Path $SourceRoot $relative
        Assert-RegularFile $path
        if ($assetText -ccontains $relative) {
            if ([IO.File]::ReadAllBytes($path) -contains 0) { throw "Binary data in asset notice/manifest: $relative" }
            continue
        }
        # Tracked empty BIOS dispatch stub is deliberately allowed. Real translated bodies are not.
        if ($relative -match '(?i)(\.(gba|gb|gbc|nds|rom|bios|bin|elf|exe|dll|so|a|lib|o|obj|pdb|zip|7z|tar|gz|ttf|otf|png|jpe?g|webp|bmp|wav|mp3|ogg|sav|srm|state)$|\.private\.|(^|/)(assets|saves|private|build|dist|\.aws|\.ssh)/|bios_recompiled|bios_dispatch_table|third_age_ringbearer_data\.h|(^|/)launch\.json$)') {
            throw "Disallowed source input: $relative"
        }
        $path = Join-Path $SourceRoot $relative
        Assert-RegularFile $path
        $bytes = [IO.File]::ReadAllBytes($path)
        if ($bytes -contains 0) { throw "Binary/NUL data in source checkout: $relative" }
        # Known private embedded-data headers and generated cartridge translation names.
        if ($relative -match '(?i)(game_recompiled|cartridge/generated|third_age_native_decode\.private|third_age_portrait_patterns\.private)') {
            throw "Generated private source: $relative"
        }
    }
    Write-Host "Source input audit passed for $($files.Count) tracked files, code and notices only; portrait BMPs and font archives are excluded. This is a static audit, not a provenance clearance."
}
Assert-SourceInputs
if ($Mode -eq 'AuditSource') { return }
if (!$IsWindows) { throw 'Packaging and PE dependency verification require Windows and preinstalled MSVC dumpbin.' }
foreach ($value in @($BuildRoot, $Executable, $SdlRoot, $SdlArchive, $OutputDirectory, $Commit, $PublicRuntimeCMakeOption)) {
    if ([string]::IsNullOrWhiteSpace($value)) { throw 'All PackageBase parameters are required.' }
}
if ($Commit -notmatch '^[0-9a-f]{40}$') { throw 'Commit must be an immutable full Git SHA.' }
$head = & git -C $SourceRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $head -ne $Commit) { throw 'Package commit must equal the clean source HEAD.' }
if ($PublicRuntimeCMakeOption -notmatch '^THIRD_AGE_[A-Z0-9_]+$') { throw 'Reviewed public runtime option is required.' }
$cache = Get-Content -LiteralPath (Join-Path $BuildRoot 'CMakeCache.txt') -Raw
foreach ($entry in @("${PublicRuntimeCMakeOption}:BOOL=ON", 'THIRD_AGE_ALLOW_HEADLESS:BOOL=OFF',
    'CMAKE_MSVC_RUNTIME_LIBRARY:UNINITIALIZED=MultiThreaded', 'GBARECOMP_SELFHEAL_RECOMPILE_DEFAULT:BOOL=OFF')) {
    # CMake can promote an explicitly supplied cache entry from UNINITIALIZED to STRING.
    $pattern = '(?m)^' + [regex]::Escape($entry).Replace('UNINITIALIZED', '(?:UNINITIALIZED|STRING)') + '\r?$'
    if ($cache -notmatch $pattern) { throw "Build contract missing from cache: $entry" }
}
# Bound the value to one line: .+ also matches the carriage return in an empty CRLF entry.
$privateEntries = [regex]::Matches($cache, '(?m)^THIRD_AGE_PRIVATE_INCLUDE_DIR:[^=\r\n]+=([^\r\n]*)\r?$')
if ($privateEntries.Count -ne 1) { throw 'Expected exactly one private include directory cache entry.' }
if (![string]::IsNullOrWhiteSpace($privateEntries[0].Groups[1].Value)) { throw 'Private include directory must not be configured.' }
if ((Get-Sha256 $SdlArchive) -ne $SdlSha256) { throw 'SDL vendor archive hash mismatch.' }

# Verify the exact runtime/license bytes against the pinned archive, not a directory name.
$archive = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $SdlArchive).Path)
try {
    foreach ($relative in @('lib/x64/SDL2.dll', 'LICENSE.txt', 'README-SDL.txt')) {
        $entry = $archive.GetEntry("SDL2-2.32.10/$relative")
        if (!$entry) { throw "Missing vendor archive entry: $relative" }
        $stream = $entry.Open()
        try { $expected = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)).ToLowerInvariant() }
        finally { $stream.Dispose() }
        if ((Get-Sha256 (Join-Path $SdlRoot $relative)) -ne $expected) { throw "Modified SDL file: $relative" }
    }
} finally { $archive.Dispose() }

if (Test-Path -LiteralPath $OutputDirectory) { throw 'Output directory must not already exist.' }
$null = New-Item -ItemType Directory -Path $OutputDirectory
$name = "third-age-pc-windows-x64-preview-base-$($Commit.Substring(0,12))"
$stage = Join-Path $OutputDirectory $name
$null = New-Item -ItemType Directory -Path $stage
$allowed = [Collections.Generic.List[string]]::new()
function Copy-Allowed([string]$From, [string]$To) {
    Assert-RegularFile $From
    $destination = Join-Path $stage $To
    $null = New-Item -ItemType Directory -Path (Split-Path $destination) -Force
    Copy-Item -LiteralPath $From -Destination $destination
    $allowed.Add($To)
}
function Write-Allowed([string]$To, [string]$Text) {
    [IO.File]::WriteAllText((Join-Path $stage $To), $Text, [Text.UTF8Encoding]::new($false))
    $allowed.Add($To)
}
Copy-Allowed $Executable 'third-age-pc.exe'
Copy-Allowed (Join-Path $SdlRoot 'lib/x64/SDL2.dll') 'SDL2.dll'
Copy-Allowed (Join-Path $SdlRoot 'LICENSE.txt') 'notices/SDL2-LICENSE.txt'
Copy-Allowed (Join-Path $SdlRoot 'README-SDL.txt') 'notices/SDL2-README.txt'
$notices = [ordered]@{
    'DISTRIBUTION-NOTICE.md' = 'notices/DISTRIBUTION-NOTICE.md'
    'LICENSING-INVENTORY.md' = 'notices/LICENSING-INVENTORY.md'
    'vendor/gbarecomp/LICENSE' = 'notices/gbarecomp-LICENSE.txt'
    'vendor/gbarecomp/THIRD_PARTY_ATTRIBUTION.md' = 'notices/THIRD_PARTY_ATTRIBUTION.md'
    'vendor/gbarecomp/third_party/MPL-2.0.txt' = 'notices/MPL-2.0.txt'
    'vendor/gbarecomp/third_party/tomlpp/LICENSE' = 'notices/tomlpp-LICENSE.txt'
    'vendor/gbarecomp/external/arm-recomp-core/LICENSE' = 'notices/arm-recomp-core-LICENSE.txt'
    'vendor/gbarecomp/external/rbengine/LICENSE' = 'notices/rbengine-LICENSE.txt'
    'vendor/gbarecomp/external/recomp-net/LICENSE' = 'notices/recomp-net-LICENSE.txt'
    'vendor/gbarecomp/src/runtime/bios_hle.cpp' = 'source/mpl/bios_hle.cpp'
    'vendor/gbarecomp/src/runtime/bios_hle.h' = 'source/mpl/bios_hle.h'
}
foreach ($pair in $notices.GetEnumerator()) { Copy-Allowed (Join-Path $SourceRoot $pair.Key) $pair.Value }
# The base carries small manifests/notices only. The local assembler adds exact assets later.
Copy-Allowed (Join-Path $SourceRoot 'assets/PORTRAIT-MANIFEST.json') 'assets/PORTRAIT-MANIFEST.json'
Copy-Allowed (Join-Path $SourceRoot 'assets/PORTRAIT-NOTICE.md') 'assets/PORTRAIT-NOTICE.md'
Copy-Allowed (Join-Path $SourceRoot 'assets/FONT-MANIFEST.json') 'assets/FONT-MANIFEST.json'
Copy-Allowed (Join-Path $SourceRoot 'assets/FONT-NOTICE.md') 'extras/Ringbearer-NOTICE.txt'

# Inspect all shipped PE files, including SDL's transitive/delay-load imports.
# Never treat "found on the build runner" as proof of availability on a user's PC.
$systemDlls = @('kernel32.dll','user32.dll','gdi32.dll','advapi32.dll','shell32.dll','ole32.dll',
    'oleaut32.dll','comdlg32.dll','comctl32.dll','winmm.dll','imm32.dll','version.dll','setupapi.dll',
    'ws2_32.dll','iphlpapi.dll','ntdll.dll','bcrypt.dll','crypt32.dll','secur32.dll','rpcrt4.dll',
    'dwmapi.dll','uxtheme.dll','shlwapi.dll','cfgmgr32.dll','winspool.drv','msvcrt.dll','ucrtbase.dll')
$peNames = @('third-age-pc.exe', 'SDL2.dll')
$imports = [ordered]@{}
foreach ($pe in $peNames) {
    $path = Join-Path $stage $pe
    $headers = & dumpbin.exe /nologo /headers $path 2>&1
    if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch '(?im)^\s*8664 machine \(x64\)') { throw "Not an x64 Windows PE: $pe" }
    $listing = & dumpbin.exe /nologo /dependents $path 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect DLL imports: $pe" }
    $deps = @($listing | ForEach-Object {
        if ($_ -match '(?i)^\s+([a-z0-9_.-]+\.(dll|drv))\s*$') { $Matches[1].ToLowerInvariant() }
    } | Sort-Object -Unique)
    if (!$deps.Count) { throw "No imports recognized; inspect dumpbin output: $pe" }
    foreach ($dll in $deps) {
        if ($dll -in $peNames -or $dll -in $systemDlls -or $dll -match '^api-ms-win-(core|crt|security)-[a-z0-9-]+\.dll$') { continue }
        throw "Unbundled/unreviewed DLL import in ${pe}: $dll. Do not copy arbitrary runner DLLs."
    }
    $imports[$pe] = $deps
}
if ('sdl2.dll' -notin $imports['third-age-pc.exe']) { throw 'Executable does not import SDL2; possible headless/wrong target.' }
# Exercise the Windows loader and argument parser without opening a GUI or reading
# game inputs. --help currently continues startup, so deliberately reject a flag.
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = Join-Path $stage 'third-age-pc.exe'
$start.WorkingDirectory = $stage
$start.ArgumentList.Add('--ci-package-loader-probe')
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$start.Environment['PATH'] = "$env:SystemRoot/System32;$env:SystemRoot"
foreach ($key in @($start.Environment.Keys)) {
    if ($key -match '^(GBARECOMP_|THIRD_AGE_)') { $null = $start.Environment.Remove($key) }
}
$process = [Diagnostics.Process]::new()
$process.StartInfo = $start
try {
    if (!$process.Start()) { throw 'Cannot start loader probe.' }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (!$process.WaitForExit(15000)) { $process.Kill($true); throw 'Loader probe timed out.' }
    $diagnostic = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
    if ($process.ExitCode -ne 1 -or $diagnostic -notmatch 'unknown argument --ci-package-loader-probe') {
        throw "Windows loader/CLI probe failed (exit $($process.ExitCode)); no preview may be uploaded."
    }
} finally { $process.Dispose() }
Write-Allowed 'README.txt' @"
Third Age PC - Windows x64 PREVIEW

CODE-ONLY CI BASE: portraits are NOT included. Assemble the final ZIP before delivery.

Extract the entire ZIP to a NEW writable directory and run third-age-pc.exe.
No compiler, Python, SDK or dependency installer is required.
Native Windows gameplay acceptance is still PENDING; this is a preview.

Select your own BIOS and game ROM in the Windows file pickers. Incorrect inputs
are rejected. Required SHA-1 hashes and sizes:
BIOS: 300c20df6731a33952ded8c436f7f186d25d3492 (16,384 bytes)
ROM:  0a7a704d2ba167eb2dc03ad5dcfa0f09ea982a54 (16,777,216 bytes)
Selected paths are remembered in bios.cfg and rom.cfg beside the executable.
Keep those files and all personal inputs private.

The interface uses an installed system font by default. An optional user-owned
font can be placed at assets/fonts/ringbearer.ttf beside the executable, or
selected by setting THIRD_AGE_FONT to a local TrueType font path before launch.
No custom font is required. Create assets/fonts only for that optional font.
An invalid explicit font path is reported as an error.

New saves default to saves/third-age.sav beside the executable. Do not merge this
preview into an existing campaign/save directory or overwrite an original save.
The final assembled download includes the 68 exact approved BMP portraits and
an optional unchanged complete Ringbearer archive. This CI base contains only
their manifests/notices, NOT the images or font archive. Asset rights are separate
from code licenses; see assets/PORTRAIT-NOTICE.md and extras/Ringbearer-NOTICE.txt.
The optional font archive is not automatically extracted, installed or loaded.
No ROM, BIOS, personal save, film video/music, bare TTF, or game-derived
translation is included in either package.
Detailed instructions:
https://github.com/brandongillespie17-jpg/third-age-pc-recomp/blob/$Commit/docs/WINDOWS-RELEASE.md

This package contains SDL2 2.32.10 (zlib license). Preserve notices/ and source/.
Framework terms and all third-party notices remain in force; no blanket license
is granted for local contributions or excluded assets. Exact modified MPL source
is included in source/mpl. The complete corresponding repository revision is:
https://github.com/brandongillespie17-jpg/third-age-pc-recomp/tree/$Commit

Build/dependency metadata and import audit: BUILD-INFO.json
Verify packaged file hashes using SHA256SUMS.txt. Keep personal inputs private.
"@
$info = [ordered]@{
    status = 'preview; native Windows gameplay not tested'
    commit = $Commit
    target = 'third-age-pc'; architecture = 'x64'; configuration = 'Release'
    publicRuntimeCMakeOption = $PublicRuntimeCMakeOption
    msvcRuntime = 'MultiThreaded (/MT); unexpected runtime DLLs fail packaging'
    packageKind = 'code-only-base'; ciAuditPassed = $true
    fontArchiveIncluded = $false; fontManifestSha256 = $FontManifestSha256
    portraitCount = 0; approvedPortraitCount = 68; portraitManifestSha256 = $PortraitManifestSha256
    sdlVersion = '2.32.10'; sdlUrl = $SdlUrl; sdlArchiveSha256 = $SdlSha256
    runnerImage = $env:ImageOS; runnerImageVersion = $env:ImageVersion
    cmake = (& cmake --version | Select-Object -First 1)
    compiler = ((& cl.exe 2>&1 | ForEach-Object { "$_" }) -join "`n")
    importedDlls = $imports
    tests = @('clean tracked code-only source audit and pinned portrait manifest', 'SDL vendor SHA256 and shipped bytes', 'x64 PE and recursive shipped DLL imports', 'isolated PATH Windows loader/invalid-argument probe (expected exit 1)', 'ZIP entry allowlist and round-trip file hashes')
    limitations = @('No GUI/gameplay acceptance', 'Static scans cannot prove provenance or detect every embedded payload', 'Hosted runner tools change; byte-identical compiler output is not promised')
}
$global:LASTEXITCODE = 0
Write-Allowed 'BUILD-INFO.json' ($info | ConvertTo-Json -Depth 6)
$hashLines = @($allowed | Sort-Object | ForEach-Object { "$(Get-Sha256 (Join-Path $stage $_))  $_" })
Write-Allowed 'SHA256SUMS.txt' (($hashLines -join "`n") + "`n")
$actual = @(Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object { [IO.Path]::GetRelativePath($stage, $_.FullName).Replace('\','/') })
if (Compare-Object ($allowed | Sort-Object) ($actual | Sort-Object)) { throw 'Staging directory differs from exact allowlist.' }

# Stable order and timestamps make the ZIP deterministic for identical file bytes.
$zipPath = Join-Path $OutputDirectory "$name.zip"
$zip = [IO.Compression.ZipFile]::Open($zipPath, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($relative in ($allowed | Sort-Object)) {
        $entry = $zip.CreateEntry("$name/$relative", [IO.Compression.CompressionLevel]::Optimal)
        $entry.LastWriteTime = [DateTimeOffset]::new(2000,1,1,0,0,0,[TimeSpan]::Zero)
        $outStream = $entry.Open(); $inStream = [IO.File]::OpenRead((Join-Path $stage $relative))
        try { $inStream.CopyTo($outStream) } finally { $inStream.Dispose(); $outStream.Dispose() }
    }
} finally { $zip.Dispose() }
# Read the final archive itself, not only staging, before it can be uploaded.
$zip = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $expectedNames = @($allowed | ForEach-Object { "$name/$_" } | Sort-Object)
    if (Compare-Object $expectedNames @($zip.Entries.FullName | Sort-Object)) { throw 'ZIP entry allowlist mismatch.' }
    foreach ($entry in $zip.Entries) {
        $relative = $entry.FullName.Substring($name.Length + 1)
        $stream = $entry.Open()
        try { $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)).ToLowerInvariant() }
        finally { $stream.Dispose() }
        if ($hash -ne (Get-Sha256 (Join-Path $stage $relative))) { throw "ZIP hash mismatch: $relative" }
    }
} finally { $zip.Dispose() }
[IO.File]::WriteAllText("$zipPath.sha256", "$(Get-Sha256 $zipPath)  $name.zip`n", [Text.UTF8Encoding]::new($false))
Write-Host "Audited code-only base ZIP: $zipPath"
