//-----------------------------------------------------------------------------
// File: random.h
//
// Desc: The one random-number idiom every screen saver in this tree uses:
//       rand() scaled into [0, 1).  It only ever appears inlined in the
//       shipped images (as a multiplication by 1/32768), so the name is ours.
//-----------------------------------------------------------------------------
#ifndef _HELPER_RANDOM_H
#define _HELPER_RANDOM_H

#include <stdlib.h>

// Uniform random number in [0, 1)
inline float RandomFloat()
{
    return (float)rand() / (float)( RAND_MAX + 1 );
}

#endif // _HELPER_RANDOM_H
