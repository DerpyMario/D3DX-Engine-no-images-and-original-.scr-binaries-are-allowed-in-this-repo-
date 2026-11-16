//-----------------------------------------------------------------------------
// File: targa.h
//
// Desc: Sequential reader for true-colour Targa (.tga) images held in memory.
//       Handles image type 2 (uncompressed) and 10 (run-length encoded), 24 or
//       32 bits per pixel.
//
//       Original object:
//           shell\osshell\control\scrnsave\helper\objfre\amd64\targa.obj
//-----------------------------------------------------------------------------
#ifndef _HELPER_TARGA_H
#define _HELPER_TARGA_H

#include <windows.h>

namespace Targa
{
    #pragma pack(push, 1)
    struct TargaHeader
    {
        BYTE idLength;
        BYTE colorMapType;
        BYTE imageType;
        WORD colorMapOrigin;
        WORD colorMapLength;
        BYTE colorMapDepth;
        WORD xOrigin;
        WORD yOrigin;
        WORD width;
        WORD height;
        BYTE bitsPerPixel;
        BYTE imageDescriptor;       // bit 5 set: first row is the top row

        TargaHeader()
        {
            idLength        = 0;
            colorMapType    = 0;
            imageType       = 0;
            colorMapOrigin  = 0;
            colorMapLength  = 0;
            colorMapDepth   = 0;
            xOrigin         = 0;
            yOrigin         = 0;
            width           = 0;
            height          = 0;
            bitsPerPixel    = 0;
            imageDescriptor = 0;
        }
    };
    #pragma pack(pop)

    enum
    {
        TGA_TRUECOLOR      = 2,
        TGA_TRUECOLOR_RLE  = 10,
        TGA_ORIGIN_TOP     = 0x20   // TargaHeader::imageDescriptor
    };

    class TargaReader
    {
    public:
        TargaReader( const BYTE* pData );

        // Returns the next pixel of the image, in file order.  Fails for image
        // types other than (RLE) true-colour.
        bool GetRGBA( BYTE& r, BYTE& g, BYTE& b, BYTE& a );

        const TargaHeader& Header() const   { return m_Header; }
        bool  IsTopDown() const             { return ( m_Header.imageDescriptor & TGA_ORIGIN_TOP ) != 0; }

        // True once every row of the image has been returned
        bool  IsDone() const                { return m_y >= (int)m_Header.height; }

    private:
        // Reads one B, G, R (, A) pixel at the cursor
        void ReadPixel( BYTE& r, BYTE& g, BYTE& b, BYTE& a )
        {
            b = *m_pCursor++;
            g = *m_pCursor++;
            r = *m_pCursor++;
            if( m_Header.bitsPerPixel == 24 )
                a = 0xff;
            else
                a = *m_pCursor++;
        }

        TargaHeader m_Header;
        const BYTE* m_pData;        // start of the file
        const BYTE* m_pImageData;   // first byte after the header and the image ID
        const BYTE* m_pCursor;      // next byte to read
        int         m_x;            // position of the next pixel
        int         m_y;
        int         m_nPacketCount; // pixels left in the current RLE packet
        bool        m_bRunPacket;   // current packet is a run (as opposed to raw)
        BYTE        m_r;            // colour of the current run
        BYTE        m_g;
        BYTE        m_b;
        BYTE        m_a;
    };
}

#if defined(_WIN64)
static_assert( sizeof(Targa::TargaHeader) == 18,   "TargaHeader layout" );
static_assert( sizeof(Targa::TargaReader) == 0x48, "TargaReader layout" );
#endif

#endif // _HELPER_TARGA_H
