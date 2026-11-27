//============================================================================================
// noise.h -- Ken Perlin "Improved Noise" used by both Ribbons.scr and Mystify.scr
//
// Reconstructed from the static analysis of Ribbons.scr (RVA 0xC79C = fractal sum,
// RVA 0xC3E0 = single octave, RVA 0xC31C = gradient dot switch, RVA 0x18570 =
// the 256-entry permutation table; Mystify.scr carries the identical table at RVA
// 0x18560). A separate 256-float random table at RVA 0x1EC80..0x1F080 (Ribbons) is
// filled by srand(time(NULL)) + rand() at RVA 0xC2A8, guarded by a once-flag at
// RVA 0x1F244, and supplies the initial ribbon positions/velocities.
//
// Verified against the binary:
//   * permutation[0..15] = { 225, 155, 210, 108, 175, 199, 221, 144, 203, 116, 70,
//                            213, 69, 158, 33, 252 }   (Ken Perlin's original table)
//   * fade(t) = t^3 * (t * (t*6 - 15) + 10)           (constants 6.0 @0x18B74, 10.0 @0x18B78)
//   * gradient selector = perm[hash] & 0xF            (16 gradients, switch at RVA 0xC31C)
//   * octave sum: noise(p) + noise(2p)/2 + noise(4p)/4 (scales 1.0, 2.0, 4.0 @0xC7F9..0xC801)
//============================================================================================
#pragma once

namespace D3DSaverNoise {

// One-time initialisation: seeds the CRT rand() generator the way the original does
// (msvcrt!srand with the wall clock) so that rand()-driven ribbon placement follows
// the same distribution. The permutation table itself is a fixed .rdata constant.
void Initialize(unsigned seed);

// Fractal noise used by the camera / ribbon-head wobble: 3 octaves (1, 2, 4).
float Fractal3(float x, float y, float z);

// Single-octave gradient noise (reconstruction of RVA 0xC3E0).
float Perlin3(float x, float y, float z);

// Returns true once the once-flag (RVA 0x1F244) equivalent has been set.
bool IsInitialized();

} // namespace D3DSaverNoise
