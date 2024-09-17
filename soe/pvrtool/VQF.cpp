/*************************************************
 VQF Loading/Saving and VQF to PVR conversion
 
   Note: The LoadVQF function has only been
   tested on *one file* and will probably
   go bang at the first available opportunity

  To Do
  
    * Test LoadVQF!

**************************************************/

#pragma pack( push, 1 )

#include <stdio.h>
#include <string.h>
#include "VQF.h"
#include "PVR.h"
#include "Util.h"
#include "Picture.h"
#include "Image.h"
#include "Twiddle.h"




//////////////////////////////////////////////////////////////////////
// Load the given VQF file into the mmrgba object
//////////////////////////////////////////////////////////////////////
bool LoadVQF( const char* pszFilename, MMRGBA& mmrgba, unsigned long int dwFlags )
{
    //open the file
    FILE* file = fopen( pszFilename, "rb" );
    if( file == NULL ) return false;

    //load the file into a buffer
    fseek( file, 0, SEEK_END ); int nFileLength = ftell( file ); fseek( file, 0, SEEK_SET );
    unsigned char* pBuffer = (unsigned char*)malloc( nFileLength );
    if( (int)fread( pBuffer, 1, nFileLength, file ) < nFileLength ) { fclose(file); free(pBuffer); return false; };
    fclose(file);

    unsigned char* pPtr = pBuffer;

    //read header
    VQFHeader* pHeader = (VQFHeader*)pPtr;
    pPtr += sizeof(VQFHeader);


    bool bAlpha = false;
    ImageColourFormat icf = ICF_565;
    switch( pHeader->nMapType & 0x3F )
    {
        case VQF_MAPTYPE_1555: bAlpha = true; icf = ICF_1555; break;
        case VQF_MAPTYPE_555:                 icf = ICF_555; break;
        case VQF_MAPTYPE_565:                 icf = ICF_565; break;
        case VQF_MAPTYPE_4444: bAlpha = true; icf = ICF_4444; break;
        case VQF_MAPTYPE_YUV422:              icf = ICF_YUV422; break;
        default: ShowErrorMessage( "Unsupported colour format" ); free(pBuffer); return false;
    }

    //extract info from header
    bool bMipMaps = ( ( pHeader->nMapType & VQF_MAPTYPE_MIPMAPPED ) == VQF_MAPTYPE_MIPMAPPED );
    int nCodeBookSize;
    switch( pHeader->nCodeBookSize )
    {
        case 0:  nCodeBookSize = 8;  break;
        case 1:  nCodeBookSize = 16; break;
        case 2:  nCodeBookSize = 32; break;
        case 3:  nCodeBookSize = 64; break;
        case 4:  nCodeBookSize = 128; break;
        case 5:  nCodeBookSize = 256; break;
        default: ShowErrorMessage( "Unsupported codebook size" ); free( pBuffer ); return false;
    }
    int nDimension;
    switch( pHeader->nTextureSize )
    {
        case 0: nDimension = 32; break;
        case 1: nDimension = 64; break;
        case 2: nDimension = 128; break;
        case 3: nDimension = 256; break;
        case 4: nDimension = 8; break;
        case 5: nDimension = 16; break;
        case 6: nDimension = 512; break;
        case 7: nDimension = 1024; break;
        default: ShowErrorMessage( "Unknown image size" ); free(pBuffer); return false;
    }
    mmrgba.Init( MMINIT_ALLOCATE|MMINIT_RGB|(( bAlpha && (dwFlags & LPF_LOADALPHA))?MMINIT_ALPHA:0) | (bMipMaps?MMINIT_MIPMAP:0), nDimension, nDimension );
    int nNumMipMaps = mmrgba.nMipMaps;

    //get texture data
    VQFCodeBookEntry* pCodeBook = NULL;
    pCodeBook = (VQFCodeBookEntry*)pPtr;
    pPtr += sizeof(VQFCodeBookEntry) * nCodeBookSize;


    //skip over 1x1 placeholder
    if( bMipMaps ) pPtr+=1;

    //unpack image
    int iMipMap = bMipMaps ? mmrgba.nMipMaps - 2 : 0;
    int nTempWidth = bMipMaps ? 2 : mmrgba.nWidth;
    int nTempHeight = nTempWidth;
    while( iMipMap >= 0 )
    {
        unsigned char* pRGB = mmrgba.pRGB[iMipMap];
        unsigned char* pAlpha = mmrgba.pAlpha ? mmrgba.pAlpha[iMipMap] : NULL;

        //compute twiddle bit values
        unsigned long int mask, shift;
        ComputeMaskShift( nTempWidth / 2, nTempHeight / 2, mask, shift );

        //read the image
        int nWrite = 0, nMax = (nTempWidth / 2) * (nTempHeight / 2);
        int x = 0, y = 0;
        while( nWrite < nMax )
        {
            int iPos = CalcUntwiddledPos( x / 2, y / 2, mask, shift );

            //unpack the twiddled 2x2 block
            bool bDown = true;
            for( int iTexel = 0; iTexel < 4; iTexel++ )
            {
                //get the texel
                unsigned short int Texel = pCodeBook[ pPtr[iPos] ].Texel[iTexel];

                //determine where to write
                int iAlphaWrite = (x + (y * nTempWidth));
                int iWrite = iAlphaWrite * 3;

                //write the values into the buffer
                unsigned char *pa, *pr, *pg, *pb;
                pa = ( pAlpha && bAlpha ) ? &pAlpha[ iAlphaWrite ] : NULL;
                pb = &pRGB[iWrite++];
                pg = &pRGB[iWrite++];
                pr = &pRGB[iWrite++];
                UnpackTexel( x, y, Texel, pa, pr, pg, pb, icf );

                //adjust x & y so we step through the image in a twiddled way
                if( bDown ) y += 1; else { y -= 1; x += 1; }
                bDown = !bDown;
            }

            //update write position
            nWrite++;
            if( x >= nTempWidth ) { x = 0; y += 2; }
        }

        //move pointer over the mipmap we just unpacked
        pPtr += nMax;

        //update values
        iMipMap--;
        nTempWidth *= 2;
        nTempHeight *= 2;
    }

    //set description
    sprintf( mmrgba.szDescription, "VQF texture %dx%d", mmrgba.nWidth, mmrgba.nHeight );

    //clean up and return
    free( pBuffer );
    return true;
}





#pragma pack( pop )
