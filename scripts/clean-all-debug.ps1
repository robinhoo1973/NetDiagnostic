# clean-all-debug.ps1 — comment out ALL debug output, remove DebugSwitch.h
$proj = "C:\Users\RemotePC\Documents\NetDiagnostic - QT"
$ErrorActionPreference = "Stop"

# files with trace modifications
$trace_files = @(
    "src/app/AppState.cpp",
    "src/main.cpp",
    "src/engine/runner/NetworkProbe.cpp",
    "src/engine/diagnostic/G4RemoteHost.cpp",
    "src/engine/diagnostic/G1G2G3Native.cpp",
    "src/engine/diagnostic/DiagnosticEngine.cpp"
)

foreach ($rel in $trace_files) {
    $f = Join-Path $proj $rel
    if (-not (Test-Path $f)) { Write-Host "SKIP $f"; continue }
    $c = [IO.File]::ReadAllText($f)

    # 1. Remove duplicate DebugSwitch.h includes
    $c = $c -replace '\s*#include\s+"util/DebugSwitch\.h"\r?\n', "`r`n"

    # 2. TRACE("... → // fprintf(stderr, "[TRACE]"...
    $c = $c -replace '^(\s*)TRACE\("', '$1// fprintf(stderr, "[TRACE]"'

    # 3. MAIN_LOG("... → // fprintf(stderr, "[MAIN]"...
    $c = $c -replace '^(\s*)MAIN_LOG\("', '$1// fprintf(stderr, "[MAIN]"'

    # 4. Comment out remaining fprintf(stderr, ...) that aren't already commented
    $lines = $c -split "`r`n"
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line -match '^\s*fprintf\(stderr,' -and $line -notmatch '^\s*//') {
            $lines[$i] = $line -replace '^(\s*)fprintf', '$1// fprintf'
        }
    }
    $c = $lines -join "`r`n"

    # 5. Comment out qDebug() / qWarning() / qCritical() / qInfo() lines
    $lines = $c -split "`r`n"
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line -match '\bq(Debug|Warning|Critical|Info)\b' -and $line -notmatch '^\s*//') {
            $lines[$i] = $line -replace '^(\s*)', '$1// '
        }
    }
    $c = $lines -join "`r`n"

    [IO.File]::WriteAllText($f, $c, [Text.Encoding]::UTF8)
    Write-Host "Cleaned: $rel"
}

# 6. Delete DebugSwitch.h
$dh = Join-Path $proj "src/util/DebugSwitch.h"
if (Test-Path $dh) { Remove-Item $dh -Force; Write-Host "Removed DebugSwitch.h" }

# 7. Clean CMakeLists.txt — remove ENABLE_TRACE blocks
$cm = Join-Path $proj "CMakeLists.txt"
$cc = [IO.File]::ReadAllText($cm)
$cc = $cc -replace '\r?\n# Debug trace switch[^\n]*\r?\noption\(ENABLE_TRACE[^)]*\)\r?\n\r?\n', "`r`n"
$cc = $cc -replace '\r?\nif\(NOT ENABLE_TRACE\)\r?\n    target_compile_definitions[^\n]*ND_DISABLE_TRACE[^\n]*\r?\nendif\(\)\r?\n', ''
[IO.File]::WriteAllText($cm, $cc, [Text.Encoding]::UTF8)
Write-Host "Cleaned CMakeLists.txt"

# 8. Clean build script — remove ENABLE_TRACE
$bs = Join-Path $proj "scripts/build-static.ps1"
$bc = [IO.File]::ReadAllText($bs)
$bc = $bc -replace '\s*-DENABLE_TRACE=OFF\s*\r?\n', "`r`n"
[IO.File]::WriteAllText($bs, $bc, [Text.Encoding]::UTF8)
Write-Host "Cleaned build-static.ps1"

Write-Host "All done. Debug output commented out."
