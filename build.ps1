$ErrorActionPreference = 'Stop'
$tc = 'C:\ncs\toolchains\4f5b6ad6dd'
$env:PATH = "$tc\opt\bin;$tc\opt\bin\Scripts;$tc\bin;$tc\mingw64\bin;" + $env:PATH
$env:PYTHONPATH = "$tc\opt\bin;$tc\opt\bin\Lib;$tc\opt\bin\Lib\site-packages"
$env:ZEPHYR_BASE = 'C:\ncs\v3.4.1\zephyr'
$env:ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr/gnu'
$env:ZEPHYR_SDK_INSTALL_DIR = "$tc\opt\zephyr-sdk"
& "$tc\opt\bin\python.exe" -m west build --no-sysbuild -b promicro_nrf52840/nrf52840/uf2 -d "$PSScriptRoot\build" $PSScriptRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$tc\opt\bin\python.exe" "$PSScriptRoot\check_uf2.py" "$PSScriptRoot\build\zephyr\zephyr.uf2"
exit $LASTEXITCODE
