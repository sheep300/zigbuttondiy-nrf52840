ZigButtonDIY - test scheduler ZBOSS

Fichier modifie :
  C:\ncs\zigbuttondiy\src\main.c

Fichier prj.conf fourni uniquement pour reference : il est INCHANGE.
Ne pas modifier l'overlay/les partitions.

Build :
cd C:\ncs\zigbuttondiy
west build -b promicro_nrf52840/nrf52840/uf2 `
  -d C:\ncs\zigbuttondiy\build `
  -p always .

Verification UF2 avant flash :
$uf2 = "C:\ncs\zigbuttondiy\build\zigbuttondiy\zephyr\zephyr.uf2"
$b = [System.IO.File]::ReadAllBytes($uf2)
$min = [uint32]::MaxValue
$max = [uint32]0
for ($i = 0; $i -lt $b.Length; $i += 512) {
    if ([BitConverter]::ToUInt32($b, $i) -ne 0x0A324655) { continue }
    $addr = [BitConverter]::ToUInt32($b, $i + 12)
    $size = [BitConverter]::ToUInt32($b, $i + 16)
    if ($addr -lt $min) { $min = $addr }
    if (($addr + $size) -gt $max) { $max = $addr + $size }
}
"Start : 0x{0:X8}" -f $min
"End   : 0x{0:X8}" -f $max

Attendu :
  Start = 0x00026000
  End strictement inferieur a 0x000EB000

Ne pas flasher si ces bornes ne sont pas respectees.
