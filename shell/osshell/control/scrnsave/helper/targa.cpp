//-----------------------------------------------------------------------------
// File: targa.cpp
//
// Desc: Sequential reader for true-colour Targa (.tga) images held in memory.
//
//       Original object (695 bytes of code):
//           shell\osshell\control\scrnsave\helper\objfre\amd64\targa.obj
//-----------------------------------------------------------------------------
#include "targa.h"

using namespace Targa;




//-----------------------------------------------------------------------------
// Name: TargaReader::TargaReader()
// Desc: Parses the 18-byte header (byte by byte, so the reader does not depend
//       on the alignment or the endianness of the host) and positions the
//       cursor on the first pixel.  A colour map, if there is one, is not
//       skipped.
//-----------------------------------------------------------------------------
TargaReader::TargaReader( const BYTE* pData )
{
    m_pData = pData;

    m_Header.idLength        = pData[0];
    m_pCursor = m_pImageData = pData + sizeof(TargaHeader) + m_Header.idLength;
    m_Header.colorMapType    = pData[1];
    m_Header.imageType       = pData[2];
    m_Header.colorMapOrigin  = (WORD)( pData[4] * 256 + pData[3] );
    m_Header.colorMapLength  = (WORD)( pData[6] * 256 + pData[5] );
    m_Header.colorMapDepth   = pData[7];
    m_Header.xOrigin         = (WORD)( pData[9] * 256 + pData[8] );
    m_Header.yOrigin         = (WORD)( pData[11] * 256 + pData[10] );
    m_Header.width           = (WORD)( pData[13] * 256 + pData[12] );
    m_Header.height          = (WORD)( pData[15] * 256 + pData[14] );
    m_Header.bitsPerPixel    = pData[16];
    m_Header.imageDescriptor = pData[17];

    m_pCursor      = m_pImageData;
    m_x            = 0;
    m_y            = 0;
    m_nPacketCount = 0;
    m_bRunPacket   = false;
    m_r = m_g = m_b = m_a = 0;
}




//-----------------------------------------------------------------------------
// Name: TargaReader::GetRGBA()
//-----------------------------------------------------------------------------
bool TargaReader::GetRGBA( BYTE& r, BYTE& g, BYTE& b, BYTE& a )
{
    if( m_Header.imageType == TGA_TRUECOLOR )
    {
        ReadPixel( r, g, b, a );
    }
    else if( m_Header.imageType == TGA_TRUECOLOR_RLE )
    {
        if( m_nPacketCount == 0 )
        {
            // Start of a new packet.  The header byte holds the packet type in
            // its top bit and (count - 1) in the other seven; either kind of
            // packet starts with a pixel.
            BYTE bPacket = *m_pCursor++;
            m_nPacketCount = bPacket & 0x7f;
            m_bRunPacket   = ( bPacket >> 7 ) != 0;

            ReadPixel( m_r, m_g, m_b, m_a );
            r = m_r;
            g = m_g;
            b = m_b;
            a = m_a;
        }
        else
        {
            if( m_bRunPacket )
            {
                r = m_r;
                g = m_g;
                b = m_b;
                a = m_a;
            }
            else
            {
                ReadPixel( r, g, b, a );
            }
            m_nPacketCount--;
        }
    }
    else
    {
        return false;
    }

    m_x++;
    if( m_x >= (int)m_Header.width )
    {
        m_x = 0;
        m_y++;
    }
    return true;
}
