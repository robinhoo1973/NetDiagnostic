<#
.SYNOPSIS
    NetDiagnostic-QT Windows x64 Static Qt6 Build
.DESCRIPTION
    Builds using MSYS2 static Qt6. All temp files in %TEMP%.
    Only final .exe output in dist/. Auto-collects required DLLs.
.PARAMETER Release
    Release mode (default)
.PARAMETER Simulator
    Build simulator variant
#>
param([switch]$Release, [switch]$Simulator)

$e = $ErrorActionPreference; $ErrorActionPreference = "Stop"
$proj = $PWD.Path
$dist = "$proj\dist"
$tmp  = "$env:TEMP\netdiag-build"
$build = "$tmp\build"
$msys = "C:\msys64\ucrt64"

Write-Host "NetDiagnostic Static Qt6 Build" -ForegroundColor White
Write-Host "  Temp: $tmp" -ForegroundColor Gray
Write-Host "  Dist: $dist" -ForegroundColor Gray

Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
Remove-Item "$dist\*.exe","$dist\*.dll" -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $build -Force | Out-Null
New-Item -ItemType Directory -Path $dist -Force | Out-Null

$p = $proj -replace '\\','/' -replace 'C:','/c'
$b = $build -replace '\\','/' -replace 'C:','/c'
$d = $dist -replace '\\','/' -replace 'C:','/c'

$script = @'
#!/bin/bash
set -e
cd BUILD_PLACEHOLDER
export PATH=/ucrt64/bin:/mingw64/bin:$PATH
export PKG_CONFIG_PATH=/ucrt64/lib/pkgconfig:/ucrt64/share/pkgconfig

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/ucrt64/qt6-static;/ucrt64" \
  -DQt6_DIR=/ucrt64/qt6-static/lib/cmake/Qt6 \
  -DBUILD_SIMULATOR=SIM_PLACEHOLDER \
  -DCMAKE_EXE_LINKER_FLAGS="-static" \
  "PROJ_PLACEHOLDER"
ninja

cp net_diagnostic.exe "DIST_PLACEHOLDER/net_diagnostic-win-x86_64.exe"
if [ -f net_diagnostic_sim.exe ]; then cp net_diagnostic_sim.exe "DIST_PLACEHOLDER/net_diagnostic_sim-win-x86_64.exe"; fi

# Recursive DLL collection
M=/ucrt64/bin
> /tmp/copied.txt
copy_deps() {
    local deps=$(objdump -p "$1" 2>/dev/null | grep "DLL Name" | awk "{print \$3}" | grep -viE "api-ms|KERNEL|ADVAPI|GDI32|USER32|SHELL|ole32|WS2|ntdll|bcrypt|d3d|dxgi|DNSAPI|IPHLPAPI|SETUPAPI|WINHTTP|SHCORE|SHLWAPI|VERSION|WINMM|wlanapi|IMM32|dwmapi|DWrite|ncrypt|NETAPI|AUTHZ|UxTheme|comdlg32|CRYPT32|Secur32|USERENV|OLEAUT32|WTSAPI32|RPCRT4|USP10|WLDAP32")
    for dll in $deps; do
        grep -qxF "$dll" /tmp/copied.txt && continue
        [ -f "$M/$dll" ] && cp "$M/$dll" "DIST_PLACEHOLDER/" && echo "  $dll" && echo "$dll" >> /tmp/copied.txt
        [ -f "DIST_PLACEHOLDER/$dll" ] && copy_deps "DIST_PLACEHOLDER/$dll"
    done
}
copy_deps "DIST_PLACEHOLDER/net_diagnostic-win-x86_64.exe"
echo "DLLs: $(wc -l < /tmp/copied.txt)"
echo "DONE"
'@

$script = $script.Replace('BUILD_PLACEHOLDER', $b)
$script = $script.Replace('PROJ_PLACEHOLDER', $p)
$script = $script.Replace('DIST_PLACEHOLDER', $d)
$script = $script.Replace('SIM_PLACEHOLDER', ($Simulator.IsPresent.ToString().ToLower()))
$sim2 = if ($Simulator.IsPresent) { 'true' } else { 'false' }
$script = $script.Replace('SIM_PLACEHOLDER2', $sim2)

$script | Out-File -Encoding ASCII "$tmp\build.sh"

Write-Host "Building..." -ForegroundColor Cyan
cmd /c "C:\msys64\usr\bin\bash.exe -l $($tmp.Replace('\','/') -replace 'C:','/c')/build.sh 2>&1"

Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Output:" -ForegroundColor Green
Get-ChildItem $dist | ForEach-Object {
    $mb = if ($_.Length -gt 1048576) { "$([math]::Round($_.Length/1MB,1)) MB" } else { "$([math]::Round($_.Length/1KB,1)) KB" }
    Write-Host "  $($_.Name)  $mb"
}
$ErrorActionPreference = $e
