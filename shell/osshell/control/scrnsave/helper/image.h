//-----------------------------------------------------------------------------
// File: image.h
//
// Desc: Sniffs the type and the dimensions of an image file held in memory.
//
//       Original object:
//           shell\osshell\control\scrnsave\helper\objfre\amd64\image.obj
//-----------------------------------------------------------------------------
#ifndef _HELPER_IMAGE_H
#define _HELPER_IMAGE_H

#include <windows.h>

namespace Image
{
    enum ImageTypeEnum
    {
        IMAGE_UNKNOWN = 0,
        IMAGE_PNG     = 1,
        IMAGE_TGA     = 2,      // uncompressed or RLE true-colour Targa
        IMAGE_BMP     = 3
    };

    bool          __cdecl IsPNG( const BYTE* pData );
    bool          __cdecl GetWidthAndHeightPNG( const BYTE* pData, UINT& uWidth, UINT& uHeight );
    ImageTypeEnum __cdecl GetImageType( const BYTE* pData );
    bool          __cdecl GetWidthAndHeight( const BYTE* pData, UINT& uWidth, UINT& uHeight );
}

#endif // _HELPER_IMAGE_H
