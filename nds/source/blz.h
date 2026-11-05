// Bottom-LZ (BLZ) coding used for DS ARM9 binaries and overlays. Port of blz.cs (CUE's blz.c,
// GPL-3.0-or-later), normal mode only. The encoder finds matches through a hash chain instead
// of the original brute-force scan but picks exactly the same match (nearest position among the
// longest), so its output is byte-identical to the PC tool's and is fast enough for a DS.
#pragma once
#include <cstddef>
#include <cstdint>

namespace blz {

// Returns a malloc'd decoded buffer (caller frees) or nullptr when the data is not BLZ.
uint8_t *decode(uint8_t *pak, uint32_t pakLen, uint32_t *outLen);   // inverts `pak` in a tail region, restored before returning

// Encodes `raw` (modified temporarily, restored before returning). `arm9` selects the ARM9
// secure-area rule. Returns a malloc'd buffer (caller frees) or nullptr when out of memory.
uint8_t *encode(uint8_t *raw, uint32_t rawLen, bool arm9, bool quiet, uint32_t *outLen);

}  // namespace blz
