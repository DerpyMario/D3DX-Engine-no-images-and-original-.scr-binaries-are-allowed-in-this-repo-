//-----------------------------------------------------------------------------
// File: image.cpp
//
// Desc: Sniffs the type and the dimensions of an image file held in memory.
//
//       Original object (675 bytes of code):
//           shell\osshell\control\scrnsave\helper\objfre\amd64\image.obj
//-----------------------------------------------------------------------------
#include "image.h"

// Targa image types the loader understands
#define TGA_TYPE_TRUECOLOR      2
#define TGA_TYPE_TRUECOLOR_RLE  10




//-----------------------------------------------------------------------------
// Name: IsBMP()
// Desc: (inlined in the shipped code)
//-----------------------------------------------------------------------------
static inline bool IsBMP( const BYTE* pData )
{
    const char signature[2] = { 'B', 'M' };

    for( UINT i = 0; i < sizeof(signature); i++ )
    {
        if( signature[i] != (char)pData[i] )
            return false;
    }
    return true;
}




//-----------------------------------------------------------------------------
// Name: IsTGA()
// Desc: A Targa file has no signature; accept anything without an ID field
//       whose image type is (RLE) true-colour.  (inlined in the shipped code)
//-----------------------------------------------------------------------------
static inline bool IsTGA( const BYTE* pData )
{
    return pData[0] == 0 &&
           ( pData[2] == TGA_TYPE_TRUECOLOR || pData[2] == TGA_TYPE_TRUECOLOR_RLE );
}




//-----------------------------------------------------------------------------
// Name: Image::IsPNG()
//-----------------------------------------------------------------------------
bool __cdecl Image::IsPNG( const BYTE* pData )
{
    const char signature[8] = { '\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n' };

    for( UINT i = 0; i < sizeof(signature); i++ )
    {
        if( signature[i] != (char)pData[i] )
            return false;
    }
    return true;
}




//-----------------------------------------------------------------------------
// Name: Image::GetWidthAndHeightPNG()
// Desc: Reads the (big-endian) size out of the IHDR chunk, which must be the
//       first chunk of the file.
//-----------------------------------------------------------------------------
bool __cdecl Image::GetWidthAndHeightPNG( const BYTE* pData, UINT& uWidth, UINT& uHeight )
{
    if( !IsPNG( pData ) )
        return false;

    const BYTE chunk[4] = { 'I', 'H', 'D', 'R' };
    for( UINT i = 0; i < sizeof(chunk); i++ )
    {
        if( chunk[i] != pData[12 + i] )
            return false;
    }

    uWidth  = ( pData[16] << 24 ) + ( pData[17] << 16 ) + ( pData[18] << 8 ) + pData[19];
    uHeight = ( pData[20] << 24 ) + ( pData[21] << 16 ) + ( pData[22] << 8 ) + pData[23];
    return true;
}




//-----------------------------------------------------------------------------
// Name: Image::GetImageType()
//-----------------------------------------------------------------------------
Image::ImageTypeEnum __cdecl Image::GetImageType( const BYTE* pData )
{
    if( IsPNG( pData ) )
        return IMAGE_PNG;

    if( IsBMP( pData ) )
        return IMAGE_BMP;

    if( IsTGA( pData ) )
        return IMAGE_TGA;

    return IMAGE_UNKNOWN;
}




//-----------------------------------------------------------------------------
// Name: Image::GetWidthAndHeight()
//-----------------------------------------------------------------------------
bool __cdecl Image::GetWidthAndHeight( const BYTE* pData, UINT& uWidth, UINT& uHeight )
{
    switch( GetImageType( pData ) )
    {
        case IMAGE_PNG:
            return GetWidthAndHeightPNG( pData, uWidth, uHeight );

        case IMAGE_TGA:
            if( !IsTGA( pData ) )
                return false;
            uWidth  = ( pData[13] << 8 ) + pData[12];
            uHeight = ( pData[15] << 8 ) + pData[14];
            return true;

        case IMAGE_BMP:
            if( !IsBMP( pData ) )
                return false;
            // BITMAPFILEHEADER (14 bytes) + BITMAPINFOHEADER::biWidth / biHeight
            uWidth  = ( pData[21] << 24 ) + ( pData[20] << 16 ) + ( pData[19] << 8 ) + pData[18];
            uHeight = ( pData[25] << 24 ) + ( pData[24] << 16 ) + ( pData[23] << 8 ) + pData[22];
            return true;

        default:
            return false;
    }
}
