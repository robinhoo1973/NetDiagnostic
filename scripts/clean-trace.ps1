# Clean all trace modifications from source files
$proj = "C:\Users\RemotePC\Documents\NetDiagnostic - QT"

$files = @(
    "src/app/AppState.cpp",
    "src/main.cpp",
    "src/engine/runner/NetworkProbe.cpp", 
    "src/engine/diagnostic/G4RemoteHost.cpp",
    "src/engine/diagnostic/G1G2G3Native.cpp",
    "src/engine/diagnostic/DiagnosticEngine.cpp"
)

foreach ($rel in $files) {
    $f = Join-Path $proj $rel
    if (-not (Test-Path $f)) { continue }
    $c = [IO.File]::ReadAllText($f)
    
    # 1. Remove ALL DebugSwitch.h includes
    $c = $c -replace '#include\s+"util/DebugSwitch\.h"\r?\n', ''
    
    # 2. Revert TRACE(" -> fprintf(stderr, "[TRACE]"
    $c = $c -replace 'TRACE\("', 'fprintf(stderr, "[TRACE]"'
    
    # 3. Revert MAIN_LOG(" -> fprintf(stderr, "[MAIN]"
    $c = $c -replace 'MAIN_LOG\("', 'fprintf(stderr, "[MAIN]"'
    
    [IO.File]::WriteAllText($f, $c, [Text.Encoding]::UTF8)
    Write-Host "Cleaned: $rel"
}

# Also remove DebugSwitch.h file
$dh = Join-Path $proj "src/util/DebugSwitch.h"
if (Test-Path $dh) { Remove-Item $dh; Write-Host "Removed DebugSwitch.h" }
Write-Host "Done"