//============================================================================================
// noise.cpp -- Ken Perlin "Improved Noise" (see noise.h for provenance)
//
// The permutation table below is byte-for-byte the .rdata table of Ribbons.scr
// (RVA 0x18560) / Ribbons.scr (RVA 0x18570). It is a fixed constant in both binaries.
//============================================================================================
#include "noise.h"

#include <cstdlib>
#include <ctime>
#include <cmath>

namespace D3DSaverNoise {

namespace {

//----------------------------------------------------------------------------------------
// The 256-entry permutation table baked into Mystify.scr at RVA 0x18560 and
// Ribbons.scr at RVA 0x18570 (identical 1024 bytes in both files).
//----------------------------------------------------------------------------------------
static const unsigned char kPermutation[256] = {
    225, 155, 210, 108, 175, 199, 221, 144, 203, 116, 70, 213, 69, 158, 33, 252,
    5, 82, 173, 133, 222, 139, 174, 27, 9, 71, 90, 246, 75, 130, 91, 191,
    169, 138, 2, 151, 194, 235, 81, 7, 25, 113, 228, 159, 205, 253, 134, 142,
    248, 65, 224, 217, 22, 121, 229, 63, 89, 103, 96, 104, 156, 17, 201, 129,
    36, 8, 165, 110, 237, 117, 231, 56, 132, 211, 152, 20, 181, 111, 239, 218,
    170, 163, 51, 172, 157, 47, 80, 212, 176, 250, 87, 49, 99, 242, 136, 189,
    162, 115, 44, 43, 124, 94, 150, 16, 141, 247, 32, 10, 198, 223, 255, 72,
    53, 131, 84, 57, 220, 197, 58, 50, 208, 11, 241, 28, 3, 192, 62, 202,
    18, 215, 153, 24, 76, 41, 15, 179, 39, 46, 55, 6, 128, 167, 23, 188,
    106, 34, 187, 140, 164, 73, 112, 182, 244, 195, 227, 13, 35, 77, 196, 185,
    26, 200, 226, 119, 31, 123, 168, 125, 249, 68, 183, 230, 177, 135, 160, 180,
    12, 1, 243, 148, 102, 166, 38, 238, 251, 37, 240, 126, 64, 74, 161, 40,
    184, 149, 171, 178, 101, 66, 29, 59, 146, 61, 254, 107, 42, 86, 154, 4,
    236, 232, 120, 21, 233, 209, 45, 98, 193, 114, 78, 19, 206, 14, 118, 127,
    48, 79, 147, 85, 30, 207, 219, 54, 88, 234, 190, 122, 95, 67, 143, 109,
    137, 214, 145, 93, 92, 100, 245, 0, 216, 186, 60, 83, 105, 97, 204, 52,
};

unsigned char g_perm[512];
bool g_initialized = false;

// fade(t) = t^3 * (t*(t*6 - 15) + 10) -- constants 6.0 and 10.0 verified at
// .rdata RVA 0x18B74 / 0x18B78 in Mystify.scr (and the same offsets in Ribbons.scr).
inline float Fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float Lerp(float t, float a, float b)
{
    return a + t * (b - a);
}

// Gradient dot product -- switch on (perm & 0xF), reconstructed from RVA 0xC31C.
// The 16 cases are the classic +/-x, +/-y, +/-z axis combinations.
inline float GradDot(int hash, float x, float y, float z)
{
    switch (hash & 0xF)
    {
    case 0:  return  x;
    case 1:  return -x;
    case 2:  return  y;
    case 3:  return -y;
    case 4:  return  z;
    case 5:  return -z;
    case 6:  return  x - y;
    case 7:  return  y - x;
    case 8:  return  x + y;
    case 9:  return -x - y;
    case 10: return  y + z;
    case 11: return -y + z;
    case 12: return  z - x;
    case 13: return  x - z;
    case 14: return  z + y;
    default: return -z - y;
    }
}

int Perm(int i)
{
    return g_perm[i & 0xFF];
}

} // namespace

void Initialize(unsigned seed)
{
    if (g_initialized)
        return;

    // The original seeds the CRT generator with time(NULL) (RVA 0x6045 -> 0xC2A8 ->
    // msvcrt!srand). The CRT rand() drives the initial ribbon placement, the random
    // velocity draws on wall hits and the 256-float random table at RVA 0x1EC80.
    std::srand(static_cast<unsigned>(seed));
    for (int i = 0; i < 256; ++i)
        g_perm[i] = kPermutation[i];
    for (int i = 0; i < 256; ++i)
        g_perm[256 + i] = g_perm[i];

    g_initialized = true;
}

bool IsInitialized()
{
    return g_initialized;
}

float Perlin3(float x, float y, float z)
{
    int X = static_cast<int>(std::floor(x)) & 0xFF;
    int Y = static_cast<int>(std::floor(y)) & 0xFF;
    int Z = static_cast<int>(std::floor(z)) & 0xFF;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    float u = Fade(x);
    float v = Fade(y);
    float w = Fade(z);

    int A  = Perm(X) + Y;
    int AA = Perm(A) + Z;
    int AB = Perm(A + 1) + Z;
    int B  = Perm(X + 1) + Y;
    int BA = Perm(B) + Z;
    int BB = Perm(B + 1) + Z;

    return Lerp(w,
        Lerp(v,
            Lerp(u, GradDot(Perm(AA),     x,         y,         z), GradDot(Perm(BA),     x - 1.0f, y,         z)),
            Lerp(u, GradDot(Perm(AB),     x,         y - 1.0f,  z), GradDot(Perm(BB),     x - 1.0f, y - 1.0f,  z))),
        Lerp(v,
            Lerp(u, GradDot(Perm(AA + 1), x,         y,         z - 1.0f), GradDot(Perm(BA + 1), x - 1.0f, y,         z - 1.0f)),
            Lerp(u, GradDot(Perm(AB + 1), x,         y - 1.0f,  z - 1.0f), GradDot(Perm(BB + 1), x - 1.0f, y - 1.0f,  z - 1.0f))));
}

// RVA 0xC79C: three octaves at scale 1, 2, 4 with amplitudes 1, 1/2, 1/4.
float Fractal3(float x, float y, float z)
{
    float sum = 0.0f;
    float scale = 1.0f;
    float amp = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        sum += Perlin3(x * scale, y * scale, z * scale) * amp;
        scale *= 2.0f;
        amp *= 0.5f;
    }
    return sum;
}

} // namespace D3DSaverNoise
