"""Read-only, block-by-block nRF52840 UF2 application range validation."""
import hashlib
import struct
import sys
from pathlib import Path

def check(path):
    data = path.read_bytes()
    if not data or len(data) % 512:
        raise ValueError('Invalid UF2 size')
    intervals, numbers = [], []
    total = len(data) // 512
    for offset in range(0, len(data), 512):
        m0, m1, flags, address, size, number, count, family = struct.unpack_from('<8I', data, offset)
        end_magic = struct.unpack_from('<I', data, offset + 508)[0]
        if (m0, m1, end_magic) != (0x0a324655, 0x9e5d5157, 0x0ab16f30):
            raise ValueError('Invalid UF2 magic')
        if flags != 0x2000 or family != 0xada52840:
            raise ValueError('Unexpected flags or family')
        if not 0 < size <= 476 or not 0x26000 <= address < address + size <= 0xeb000:
            raise ValueError('Payload outside application partition')
        if count != total:
            raise ValueError('Invalid block count')
        intervals.append((address, address + size))
        numbers.append(number)
    intervals.sort()
    if sorted(numbers) != list(range(total)):
        raise ValueError('Missing or duplicate blocks')
    if any(a[1] > b[0] for a, b in zip(intervals, intervals[1:])):
        raise ValueError('Overlapping blocks')
    print(f'PASS: {path.name}: {total} blocks, [{intervals[0][0]:#08x}, {intervals[-1][1]:#08x})')
    print('SHA256:', hashlib.sha256(data).hexdigest())

if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('Usage: python check_uf2.py firmware.uf2')
    try:
        check(Path(sys.argv[1]))
    except (ValueError, OSError) as exc:
        sys.exit(f'FAIL: {exc}')
