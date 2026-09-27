$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vc='C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231'
$env:LIB='C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64'
$env:PATH="$vc/bin/Hostx64/x64;"+$env:PATH
$testBuild=Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $testBuild | Out-Null
& "$vc/bin/Hostx64/x64/cl.exe" /nologo /std:c11 /W3 /GS- /Zl /Oi- "/I$PSScriptRoot/stubs" "$PSScriptRoot/test_toggle.c" "$PSScriptRoot/host_entry.c" "/Fe$testBuild/test_toggle.exe" "/Fo$testBuild/" /link /NODEFAULTLIB /ENTRY:test_entry /SUBSYSTEM:CONSOLE kernel32.lib
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$testBuild/test_toggle.exe"
exit $LASTEXITCODE
