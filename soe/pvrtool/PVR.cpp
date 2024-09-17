/*************************************************
 PVR texture file loading/saving routines
 
   Note: The SavePVR function does not save
   VQ compressed textures. Instead, use
   "SavePVRFromVQF" from VQF.cpp

  To Fix
    * make sure the YUV image works OK on 1x1 mipmaps


  To Do
  
      * Texture Types:
          - Palettised export
          - BMP?
          - small VQ

      * Colour Formats:
          - BUMP

  To Remember

      * memcmp is used to keep things readable!!

**************************************************/
#pragma pack( push, 1 )

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "minmax.h"
#include "max_path.h"
#include "PVR.h"
#include "Picture.h"
#include "Util.h"
#include "Image.h"
#include "VQF.h"
#include "Twiddle.h"

extern unsigned char g_nOpaqueAlpha;

//////////////////////////////////////////////////////////////////////
// Loads the given PVR palette file
//////////////////////////////////////////////////////////////////////
void* LoadPVRPalette( const char* pszFilename, unsigned short int nPaletteDepth, ImageColourFormat& icfPalette )
{
    //open the file
    FILE* file = fopen( pszFilename, "rb" );
    if( file == NULL ) return NULL;

    //read & check the file header
    PVRPaletteHeader header;
    if( fread( &header, 1, sizeof(header), file ) < sizeof(header) ) { fclose(file); return NULL; }
    if( memcmp( header.PVPL, "PVPL", 4 ) != 0 ) { fclose(file); return NULL; }

    //process the the palette header
    int nPaletteEntrySize = 2;
    switch( header.nType & 0x0F )
    {
        case 0: icfPalette = ICF_1555; break;
        case 1: icfPalette = ICF_565;  break;
        case 2: icfPalette = ICF_4444; break;
        case 6: icfPalette = ICF_8888; nPaletteEntrySize = 4; break;
        default:fclose( file ); return NULL;
    }

    //determine the size that the palette must be to match the requested colour depth
    int nRequestedPaletteSize = (1 << nPaletteDepth) * nPaletteEntrySize;
    int nActualPaletteSize = header.nPaletteEntryCount * nPaletteEntrySize;
    int nBufferSize = __max( nActualPaletteSize, nRequestedPaletteSize );
   
    //allocate a buffer to hold the biggest of the file and requested palettes, and set it to black
    unsigned char* pBuffer = (unsigned char*)malloc( nBufferSize );
    memset( pBuffer, 0, nBufferSize );

    //read the palette data into a buffer
    if( (int)fread( pBuffer, 1, nActualPaletteSize, file ) < nActualPaletteSize ) { fclose(file); free(pBuffer); return NULL; };
    fclose(file);

    //return the buffer
    return (void*)pBuffer;
}


//////////////////////////////////////////////////////////////////////
// Saves the given PVR palette file
//////////////////////////////////////////////////////////////////////
bool SavePVRPalette( const char* pszFilename, unsigned short int nPaletteDepth, ImageColourFormat icfPalette, MMRGBAPAL* pPalette )
{
    //open the file
    FILE* file = fopen( pszFilename, "wb" );
    if( file == NULL ) return false;

    //build palette file header
    PVRPaletteHeader header;
    memset( &header, 0, sizeof(header) );
    memcpy( header.PVPL, "PVPL", 4 );
    int nPaletteEntrySize = 2;
    switch( icfPalette )
    {
        case ICF_1555: header.nType = 0; break;
        case ICF_565:  header.nType = 1; break;
        case ICF_4444: header.nType = 2; break;
        case ICF_8888: header.nType = 6; nPaletteEntrySize = 4; break;
        default: assert(false);
    }
    header.nPaletteEntryCount = (1 << nPaletteDepth);
    header.nPaletteDataSize = (header.nPaletteEntryCount * nPaletteEntrySize) + 8;
    
    //write header
    fwrite( &header, 1, sizeof(header), file );

    //build and write each palette entry [FIXME: write them into a buffer first!]
    for( int i = 0; i < header.nPaletteEntryCount; i++ )
    {
        switch( icfPalette )
        {
            case ICF_8888:
            {
                unsigned long int Pal32 = MAKE_8888( pPalette[i].a, pPalette[i].r, pPalette[i].g, pPalette[i].b );
                fwrite( &Pal32, 1, 4, file );
                break;
            }

            case ICF_1555:
            {
                unsigned short int Pal16 = MAKE_1555( pPalette[i].a, pPalette[i].r, pPalette[i].g, pPalette[i].b );
                fwrite( &Pal16, 1, 2, file );
                break;
            }

            case ICF_565:
            {
                unsigned short int Pal16 = MAKE_565( pPalette[i].r, pPalette[i].g, pPalette[i].b );
                fwrite( &Pal16, 1, 2, file );
                break;
            }

            case ICF_4444:
            {
                unsigned short int Pal16 = MAKE_4444( pPalette[i].a, pPalette[i].r, pPalette[i].g, pPalette[i].b );
                fwrite( &Pal16, 1, 2, file );
                break;
            }
            default: assert(false);
        }
    }

    fclose( file );
    return true;
}

















//////////////////////////////////////////////////////////////////////
// Loads the given PVR file into the mmrgba object
//////////////////////////////////////////////////////////////////////
bool LoadPVR( const char* pszFilename, MMRGBA& mmrgba, unsigned long int dwFlags )
{
    //open the file
    FILE* file = fopen( pszFilename, "rb" );
    if( file == NULL ) return ReturnError( "File open failed: ", pszFilename  );

    //load the file into a buffer
    fseek( file, 0, SEEK_END ); int nFileLength = ftell( file ); fseek( file, 0, SEEK_SET );
    unsigned char* pBuffer = (unsigned char*)malloc( nFileLength );
    if( (int)fread( pBuffer, 1, nFileLength, file ) < nFileLength ) { fclose(file); free(pBuffer); return ReturnError("Unexpected EOF: ", pszFilename ); };
    fclose(file);

    unsigned char* pPtr = pBuffer;

    //see if we've got a GBIX or not
    GlobalIndexHeader* pGBIX = (GlobalIndexHeader*)pPtr;
    if( memcmp( pGBIX->GBIX, "GBIX", 4 ) == 0 )
    {
        pPtr += sizeof(GlobalIndexHeader) + (pGBIX->nByteOffsetToNextTag-4);
    }
    else
        pGBIX = NULL;

    //read header
    PVRHeader* pHeader = (PVRHeader*)pPtr;
    pPtr += sizeof(PVRHeader);
    if( memcmp( pHeader->PVRT, "PVRT", 4 ) != 0 )
    {
        free(pBuffer);
        return ReturnError( "Not a PVR file:", pszFilename  );
    }

    //determine colour format and set masks etc.
    ImageColourFormat icf = ICF_565;
    ImageColourFormat icfPalette = ICF_NONE;
    bool bAlpha = false;
    switch( pHeader->nTextureType & 0xFF )
    {
        case KM_TEXTURE_ARGB1555: icf = ICF_1555; bAlpha = true; break;
        case KM_TEXTURE_RGB565:   icf = ICF_565; break;
        case KM_TEXTURE_ARGB4444: icf = ICF_4444; bAlpha = true; break;
        case KM_TEXTURE_YUV422:   icf = ICF_YUV422; break;
        case KM_TEXTURE_BUMP:   free( pBuffer ); return ReturnError( "Bump not supported:", pszFilename  ); //fixme: not supported
        default: free( pBuffer ); return ReturnError( "Unsupported colour format:", pszFilename  );
    }

    //determine storage method
    bool bTwiddled = false, bMipMaps = false, bVQ = false, bRectangle = false;
    int nCodeBookSize = 0;
    int nPaletteDepth = 0;

    switch( pHeader->nTextureType & 0xFF00 )
    {
        case KM_TEXTURE_TWIDDLED:           bTwiddled = true; break;
        case KM_TEXTURE_TWIDDLED_MM:        bTwiddled = true; bMipMaps = true; break;
        case KM_TEXTURE_TWIDDLED_RECTANGLE: bTwiddled = true; bRectangle = true; break;
        case KM_TEXTURE_VQ:                 bTwiddled = true; bVQ = true; nCodeBookSize = 256; break;
        case KM_TEXTURE_VQ_MM:              bTwiddled = true; bVQ = true; nCodeBookSize = 256; bMipMaps = true; break;
        case KM_TEXTURE_SMALLVQ:            bTwiddled = true; bVQ = true;                     if( pHeader->nWidth <= 16 ) nCodeBookSize = 16;  else if( pHeader->nWidth == 32 ) nCodeBookSize = 32; else if( pHeader->nWidth == 64 ) nCodeBookSize = 128; else nCodeBookSize = 256; break;
        case KM_TEXTURE_SMALLVQ_MM:         bTwiddled = true; bVQ = true; bMipMaps = true; if( pHeader->nWidth <= 16 ) nCodeBookSize = 16;  else if( pHeader->nWidth == 32 ) nCodeBookSize = 64; else nCodeBookSize = 256; break;
        case KM_TEXTURE_STRIDE:             //drop through...
        case KM_TEXTURE_RECTANGLE:          bRectangle = true; break;

        //this one isn't supposed to be supported, but if it's a square, untwiddled image, it'll still work.
        //it's included here just in case...
        case KM_TEXTURE_RECTANGLE_MM:       bRectangle = true; bMipMaps = true; break;

        
        case KM_TEXTURE_PALETTIZE4:         bTwiddled = true; nPaletteDepth = 4; break;
        case KM_TEXTURE_PALETTIZE4_MM:      bTwiddled = true; nPaletteDepth = 4; bMipMaps = true; break;
        case KM_TEXTURE_PALETTIZE8:         bTwiddled = true; nPaletteDepth = 8; break;
        case KM_TEXTURE_PALETTIZE8_MM:      bTwiddled = true; nPaletteDepth = 8; bMipMaps = true; break;

        case KM_TEXTURE_BMP:                //input only (??) [drop through to unsupported]
        default:                            free( pBuffer ); return ReturnError( "Unsupported texture type:", pszFilename  );
    }



    //load the palette if required
    void* pPalette = NULL;
    int nPaletteSize = (1 << nPaletteDepth);
    if( nPaletteDepth )
    {
        //build palette file name
        char szPaletteFile[MAX_PATH+1];
        strcpy( szPaletteFile, pszFilename );
        ChangeFileExtension( szPaletteFile, "PVP" );

        //load the palette at the requested depth
        pPalette = LoadPVRPalette( szPaletteFile, nPaletteDepth, icfPalette );
        if( pPalette == NULL )
        {
            //palette failed to load - build a grey scale one instead
            DisplayStatusMessage( "%s - palette not found. Using greyscale palette", szPaletteFile );
            icfPalette = ICF_8888;
            pPalette = malloc( 4 * nPaletteSize );
            for( int i = 0; i < nPaletteSize; i++ )
            {
                unsigned long int* pEntry = (unsigned long int*)pPalette;
                unsigned char v = (unsigned char)((256.0 / double(nPaletteSize) ) * i);
                pEntry[i] = MAKE_8888( g_nOpaqueAlpha, v, v, v );
            }
        }

        //enable alpha channel if palette has one
        if( icfPalette == ICF_1555 || icfPalette == ICF_4444 || icfPalette == ICF_8888 ) bAlpha = true;

        //set image colour format for given palette depth
        if( nPaletteDepth == 4 ) icf = ICF_PALETTE4; else icf = ICF_PALETTE8;
    }

    //allocate buffers
    mmrgba.Init( MMINIT_ALLOCATE|MMINIT_RGB|(pPalette?MMINIT_PALETTE:0)| ((bAlpha && (dwFlags & LPF_LOADALPHA))?MMINIT_ALPHA:0) | (bMipMaps?MMINIT_MIPMAP:0), pHeader->nWidth, pHeader->nHeight );

    int nMipMapCount = mmrgba.nMipMaps;
    mmrgba.icfOrigPalette = icfPalette;
    mmrgba.icfOrig = icf;
    mmrgba.nPaletteDepth = nPaletteDepth;

    //get texture data
    VQFCodeBookEntry* pCodeBook = NULL;
    if( bVQ )
    {
        pCodeBook = (VQFCodeBookEntry*)pPtr;
        pPtr += sizeof(VQFCodeBookEntry) * nCodeBookSize;
    }

    //skip over 1x1 placeholders etc.
    if( bMipMaps )
    {
        if( !bVQ )
        {
            //skip over dummy placeholders
            int nDummySize = 2;
            if( nPaletteDepth == 4 ) nDummySize = 2/*1*/; else if( nPaletteDepth == 8 ) nDummySize = 3;
            pPtr += nDummySize;
        }
    }

    //unpack image
    if( bVQ )
    {
        //unpack image
        int iMipMap = bMipMaps ? mmrgba.nMipMaps - 1 : 0;
        int nTempWidth = bMipMaps ? 1 : mmrgba.nWidth;
        int nTempHeight = nTempWidth;
        while( iMipMap >= 0 )
        {
            unsigned char* pRGB = mmrgba.pRGB ? mmrgba.pRGB[iMipMap] : NULL;
            unsigned char* pAlpha = mmrgba.pAlpha ? mmrgba.pAlpha[iMipMap] : NULL;

            //compute twiddle bit values
            unsigned long int mask, shift;
            ComputeMaskShift( nTempWidth / 2, nTempHeight / 2, mask, shift );

            //read the image
            int nWrite = 0, nMax = (nTempWidth == 1) ? 1 : (nTempWidth / 2) * (nTempHeight / 2);
            int x = 0, y = 0;
            while( nWrite < nMax )
            {
                int iPos = CalcUntwiddledPos( x / 2, y / 2, mask, shift );

                if( nTempWidth == 1 ) //special case: 1x1 vq mipmap is stored as 565 and index 0 is used
                {
                    //unpack and write the valies into the buffer
                    UnpackTexel( 0, 0, pCodeBook[ pPtr[iPos] ].Texel[0], ( pAlpha && bAlpha ) ? &pAlpha[0] : NULL, &pRGB[2], &pRGB[1], &pRGB[0], ICF_565 );
                }
                else
                {
                    //unpack the twiddled 2x2 block
                    int xoff = 0, yoff = 0;
                    int iLinear[] = { 0, 2, 1, 3 }; //this is needed so that YUV can be unpacked properly
                    for( int iTexel = 0; iTexel < 4; iTexel++ )
                    {
                        //get the texel
                        unsigned short int Texel = pCodeBook[ pPtr[iPos] ].Texel[iLinear[iTexel]];

                        //determine where to write
                        int iAlphaWrite = (x + xoff + ( (y + yoff) * nTempWidth));
                        int iWrite = iAlphaWrite * 3;

                        //unpack and write the valies into the buffer
                        unsigned char *pa, *pr, *pg, *pb;
                        pa = ( pAlpha && bAlpha ) ? &pAlpha[ iAlphaWrite ] : NULL;
                        pb = &pRGB[iWrite++];
                        pg = &pRGB[iWrite++];
                        pr = &pRGB[iWrite++];
                        UnpackTexel( x + xoff, y + yoff, Texel, pa, pr, pg, pb, icf );

                        //adjust x & y
                        xoff++;
                        if( xoff == 2 ) { xoff = 0; yoff++; }
                    }
                }

                //update write position
                nWrite++;
                x += 2;
                if( x >= nTempWidth ) { x = 0; y += 2; }
            }

            //move pointer over the mipmap we just unpacked
            pPtr += nMax;

            //update values
            iMipMap--;
            nTempWidth *= 2;
            nTempHeight *= 2;
        }
    }
    else
    {
        int iMipMap = bMipMaps ? mmrgba.nMipMaps - 1 : 0;
        int iRead = 0;
        int nTempWidth = bMipMaps ? mmrgba.nWidth >> (nMipMapCount-1) : mmrgba.nWidth;
        int nTempHeight = bMipMaps ? mmrgba.nHeight >> (nMipMapCount-1) : mmrgba.nHeight;
        while( iMipMap >= 0 )
        {
            unsigned char* pRGB = mmrgba.pRGB ? mmrgba.pRGB[iMipMap] : NULL;
            unsigned char* pAlpha = mmrgba.pAlpha ? mmrgba.pAlpha[iMipMap] : NULL;
            unsigned char* pPaletteIndices = mmrgba.pPaletteIndices ? mmrgba.pPaletteIndices[iMipMap] : NULL;

            //unpack the image
            if( bTwiddled )
            {
                //compute twiddle bit values
                unsigned long int mask, shift;
                ComputeMaskShift( nTempWidth, nTempHeight, mask, shift );

                //prepare read values
                int nWrite = 0, nMax = nTempWidth*nTempHeight;
                int x = 0, y = 0;

                //special non-VQ twiddled case: paletteised
                if( nPaletteDepth == 0 )
                {
                    //read the 16 bit image
                    while( nWrite < nMax )
                    {
                        int iPos = CalcUntwiddledPos( x, y, mask, shift );

                        //get texel and update read index
                        unsigned short int* p = (unsigned short int*)pPtr;
                        unsigned short int Texel = p[iPos];

                        //determine where to write
                        int iAlphaWrite = (x + (y * nTempWidth));
                        int iWrite = iAlphaWrite * 3;

                        unsigned char *pa, *pr, *pg, *pb;
                        pa = ( pAlpha && bAlpha ) ? &pAlpha[ iAlphaWrite ] : NULL;
                        pb = &pRGB[iWrite++];
                        pg = &pRGB[iWrite++];
                        pr = &pRGB[iWrite++];
                        UnpackTexel( x, y, Texel, pa, pr, pg, pb, icf );

                        nWrite++;
                        if( ++x >= nTempWidth ) { x = 0; y++; }
                    }

                    //move pointer over the mipmap we just unpacked
                    pPtr += 2 * nMax;
                }
                else
                {
                    //read the paletteised image
                    while( nWrite < nMax )
                    {
                        int iPos = CalcUntwiddledPos( x, y, mask, shift );
                        if( nPaletteDepth == 4 ) iPos /= 2;

                        //get palette index
                        unsigned char indexbyte = pPtr[iPos];
                        if( nPaletteDepth == 4 )
                        {
                            if( y & 1 ) indexbyte = (indexbyte >> 4); else indexbyte = indexbyte & 0x0F;
                        }

                        //determine where to write
                        pPaletteIndices[ (x + (y * nTempWidth)) ] = indexbyte;

                        nWrite++;
                        if( ++x >= nTempWidth ) { x = 0; y++; }
                    }

                    //move pointer over the mipmap we just unpacked
                    if( nPaletteDepth == 4 )
                        pPtr += nMax / 2;
                    else
                        pPtr += nMax;
                }
            }
            else
            {
                //read the image
                int nWrite = 0, nMax = nTempWidth*nTempHeight;
                int x = 0, y = 0;
                while( nWrite < nMax )
                {
                    //determine where to write
                    int iAlphaWrite = (x + (y * nTempWidth));
                    int iWrite = iAlphaWrite * 3;

                    //get texel
                    unsigned short int Texel = *((unsigned short int*)pPtr);
                    pPtr += 2;

                    unsigned char *pa, *pr, *pg, *pb;
                    pa = ( pAlpha && bAlpha ) ? &pAlpha[ iAlphaWrite ] : NULL;
                    pb = &pRGB[iWrite++];
                    pg = &pRGB[iWrite++];
                    pr = &pRGB[iWrite++];
                    UnpackTexel( x, y, Texel, pa, pr, pg, pb, icf );

                    //advance write positions
                    nWrite++;
                    if( ++x >= nTempWidth ) { x = 0; y++; }
                }
            }

            //update values
            iMipMap--;
            nTempWidth *= 2;
            nTempHeight *= 2;
        }
    }

    //store palette
    if( pPalette )
    {
        int x = 0, y = 0, nMax = ( 1 << nPaletteDepth ), nWrite = 0;
        for( int i = 0; i < nMax; i++ )
        {
            unsigned char a, r, g, b;

            //unpack palette value
            switch( icfPalette )
            {
                case ICF_1555:
                case ICF_565:
                case ICF_4444:
                {
                    unsigned short int* pPal16 = (unsigned short int*)pPalette;
                    UnpackTexel( 0, 0, pPal16[i], &a, &r, &g, &b, icfPalette );
                    break;
                }

                case ICF_8888:
                {
                    unsigned long int* pPal32 = (unsigned long int*)pPalette;
                    a = (unsigned char)(( pPal32[i] & 0xFF000000 ) >> 0x18);
                    r = (unsigned char)(( pPal32[i] & 0x00FF0000 ) >> 0x10);
                    g = (unsigned char)(( pPal32[i] & 0x0000FF00 ) >> 0x08);
                    b = (unsigned char)(  pPal32[i] & 0x000000FF );
                    break;
                }

                default:
                    a = g_nOpaqueAlpha;
                    r = g = b = 0;
                    break;
            }

            //write it into image's palette
            mmrgba.Palette[i].r = r;
            mmrgba.Palette[i].g = g;
            mmrgba.Palette[i].b = b;            
            mmrgba.Palette[i].a = a;
        }
    }


    //set description
    char szGBIX[16]; if( pGBIX ) sprintf( szGBIX, "%d", pGBIX->nGlobalIndex ); else strcpy( szGBIX, "none" );
    sprintf( mmrgba.szDescription, "PVR texture %dx%d.\nGlobal Index: %s.\nColour Format: ", mmrgba.nWidth, mmrgba.nHeight, szGBIX );
    switch( pHeader->nTextureType & 0xFF )
    {
        case KM_TEXTURE_ARGB1555: strcat( mmrgba.szDescription, "ARGB1555 | " ); break;
        case KM_TEXTURE_RGB565:   strcat( mmrgba.szDescription, "RGB565 | " ); break;
        case KM_TEXTURE_ARGB4444: strcat( mmrgba.szDescription, "ARGB4444 | " ); break;
        case KM_TEXTURE_YUV422:   strcat( mmrgba.szDescription, "YUV422 | " ); break;
        case KM_TEXTURE_BUMP:     strcat( mmrgba.szDescription, "BUMP | " ); break;
    }
    switch( pHeader->nTextureType & 0xFF00 )
    {
        case KM_TEXTURE_TWIDDLED:           strcat( mmrgba.szDescription, "TWIDDLED" ); break;
        case KM_TEXTURE_TWIDDLED_MM:        strcat( mmrgba.szDescription, "TWIDDLED_MM" ); break;
        case KM_TEXTURE_TWIDDLED_RECTANGLE: strcat( mmrgba.szDescription, "TWIDDLED_RECTANGLE" ); break;
        case KM_TEXTURE_VQ:                 strcat( mmrgba.szDescription, "VQ" ); break;
        case KM_TEXTURE_VQ_MM:              strcat( mmrgba.szDescription, "VQ_MM" ); break;
        case KM_TEXTURE_SMALLVQ:            strcat( mmrgba.szDescription, "SMALLVQ" ); break;
        case KM_TEXTURE_SMALLVQ_MM:         strcat( mmrgba.szDescription, "SMALLVQ_MM" ); break;
        case KM_TEXTURE_STRIDE:             strcat( mmrgba.szDescription, "STRIDE" ); break;
        case KM_TEXTURE_RECTANGLE:          strcat( mmrgba.szDescription, "RECTANGLE" ); break;
        case KM_TEXTURE_RECTANGLE_MM:       strcat( mmrgba.szDescription, "RECTANGLE_MM" ); break;
        case KM_TEXTURE_PALETTIZE4:         strcat( mmrgba.szDescription, "PALETTIZE4" ); break;
        case KM_TEXTURE_PALETTIZE4_MM:      strcat( mmrgba.szDescription, "PALETTIZE4_MM" ); break;
        case KM_TEXTURE_PALETTIZE8:         strcat( mmrgba.szDescription, "PALETTIZE8" ); break;
        case KM_TEXTURE_PALETTIZE8_MM:      strcat( mmrgba.szDescription, "PALETTIZE8_MM" ); break;
        case KM_TEXTURE_BMP:                strcat( mmrgba.szDescription, "BMP" ); break;
    }


    //clean up
    if( pPalette ) free( pPalette );
    free( pBuffer );
    
    return true;
}











//////////////////////////////////////////////////////////////////////
// Save the given image to the specified file (doesn't VQ compress - see VQF.cpp)
//////////////////////////////////////////////////////////////////////
bool SavePVR( const char* pszFilename, MMRGBA &mmrgba, SaveOptions* pSaveOptions )
{
    //extract settings
    bool bTwiddled = pSaveOptions->bTwiddled;
    bool bMipmaps = pSaveOptions->bMipmaps;
    int nPaletteDepth = pSaveOptions->nPaletteDepth;

    //ensure the mmrgba's paletteness is the same as the one we've been asked to save
    MMRGBA mmrgbasave;
    mmrgbasave.Copy( mmrgba );

    if( nPaletteDepth == 0 )
    {
        if( mmrgbasave.bPalette ) mmrgbasave.ConvertTo32Bit();
    }
    else
    {
        if( !mmrgbasave.bPalette )
        {
            //FIXME!
            ShowErrorMessage( "Automatic palettisation is not supported (yet). In order to save a palettised image, you have to load one in" );
            return false;
            //DisplayStatusMessage( "Warning: this program only uses a primitive palettisation algorithm. The palette might not be ideal" );
            //mmrgbasave.ConvertToPalettised( nPaletteDepth );
        }
        else
            if( mmrgbasave.nPaletteDepth > nPaletteDepth )
                DisplayStatusMessage( "Warning: source image has 8bpp palette and output format is 4bpp: ***automatic downsampling is not implemented***" );
    }


    
    //build header
    PVRHeader header;
    memcpy( header.PVRT, "PVRT", 4 );
    header.nTextureDataSize = 0;
    header.nTextureType = 0;
    header.nWidth = mmrgbasave.nWidth;
    header.nHeight = mmrgbasave.nHeight;

    //set texture type depending on settings
    ImageColourFormat icfPalette = pSaveOptions->ColourFormat;
    if( nPaletteDepth == 0 )
    {
        switch( pSaveOptions->ColourFormat )
        {
            case ICF_YUV422:header.nTextureType |= KM_TEXTURE_YUV422; break;
            case ICF_4444:  header.nTextureType |= KM_TEXTURE_ARGB4444; break;
            case ICF_555:   header.nTextureType |= KM_TEXTURE_ARGB1555; break; 
            case ICF_1555:  header.nTextureType |= KM_TEXTURE_ARGB1555; break;
            case ICF_8888:  DisplayStatusMessage( "Warning: 8888 is not supported for non-paletised images. Using 565", pszFilename ); [[fallthrough]]; //drop through...
            default:
            case ICF_565:   header.nTextureType |= KM_TEXTURE_RGB565; break;
        }
    }

    if( bMipmaps && mmrgbasave.nWidth != mmrgbasave.nHeight )
    {
        DisplayStatusMessage( "Note: Texture must be square for mipmaps. Mipmaps not generated for this image." );
        bMipmaps = false;
    }

    if( mmrgbasave.nWidth < 8 || mmrgbasave.nHeight < 8 ) return ReturnError( "Error: image dimention < 8: ", pszFilename );

    bool bStride = false;
    if( mmrgbasave.nWidth != 8    &&
        mmrgbasave.nWidth != 16   &&
        mmrgbasave.nWidth != 32   &&
        mmrgbasave.nWidth != 64   &&
        mmrgbasave.nWidth != 128  &&
        mmrgbasave.nWidth != 256  &&
        mmrgbasave.nWidth != 512  &&
        mmrgbasave.nWidth != 1024 )
    {
        if( mmrgbasave.nWidth % 32 ) return ReturnError( "Error: stride width must be a multiple of 32: ", pszFilename );
        if( mmrgbasave.nWidth < 32 || mmrgbasave.nWidth > 992 ) return ReturnError( "Error:  Stride width must be between 32 & 992: ", pszFilename );
        bStride = true;
        DisplayStatusMessage( "Width & height not powers of two. Using stride format" );
    }

    if( bStride == false && 
        (mmrgbasave.nHeight != 8    &&
         mmrgbasave.nHeight != 16   &&
         mmrgbasave.nHeight != 32   &&
         mmrgbasave.nHeight != 64   &&
         mmrgbasave.nHeight != 128  &&
         mmrgbasave.nHeight != 256  &&
         mmrgbasave.nHeight != 512  &&
         mmrgbasave.nHeight != 1024 ) ) return ReturnError( "Error: image height must be power of 2 between 8 and 1024: ", pszFilename );

    //set format type based on settings
    if( !bStride )
    {
        switch( nPaletteDepth )
        {
            case 0:
                if( bMipmaps )
                {
                    if( bTwiddled )
                        header.nTextureType |= KM_TEXTURE_TWIDDLED_MM;
                    else
                        header.nTextureType |= KM_TEXTURE_RECTANGLE_MM;
                }
                else
                {
                    if( bTwiddled )
                    {
                        if( mmrgbasave.nWidth == mmrgbasave.nHeight )
                            header.nTextureType |= KM_TEXTURE_TWIDDLED;
                        else
                            header.nTextureType |= KM_TEXTURE_TWIDDLED_RECTANGLE;
                    }
                    else
                        header.nTextureType |= KM_TEXTURE_RECTANGLE;
                }
                break;

            case 4:
                bTwiddled = true;
                if( bMipmaps )
                    header.nTextureType |= KM_TEXTURE_PALETTIZE4_MM;
                else
                    header.nTextureType |= KM_TEXTURE_PALETTIZE4;
                break;
            case 8:
                bTwiddled = true;
                if( bMipmaps )
                    header.nTextureType |= KM_TEXTURE_PALETTIZE8_MM;
                else
                    header.nTextureType |= KM_TEXTURE_PALETTIZE8;
                break;
        }
    }
    else
    {
        //it's stride - update format and turn off the fancy bits
        header.nTextureType |= KM_TEXTURE_STRIDE;
        bMipmaps = bTwiddled = false;
    }

    //calculate texture data size
    if( bMipmaps )
    {
        int nTempWidth = mmrgbasave.nWidth;
        int nTempHeight = mmrgbasave.nHeight;
        while( nTempWidth > 0 && nTempHeight > 0 )
        {
            switch( nPaletteDepth )
            {
                case 0: header.nTextureDataSize += ((nTempWidth * nTempHeight) << 1); break;
                case 4: header.nTextureDataSize += ((nTempWidth * nTempHeight) >> 1); break;
                case 8: header.nTextureDataSize += (nTempWidth * nTempHeight); break;
            }
            nTempWidth /= 2;
            nTempHeight /= 2;
        }
        header.nTextureDataSize += 8;
        switch( nPaletteDepth )
        {
            case 0:
            case 4: header.nTextureDataSize += 2; break; //2 bytes for dummy zero
            case 8: header.nTextureDataSize += 3; break;
        }
    }
    else
    {
        switch( nPaletteDepth )
        {
            case 0: header.nTextureDataSize = ((mmrgbasave.nWidth  * mmrgbasave.nHeight) << 1) + 8; break;
            case 4: header.nTextureDataSize = ((mmrgbasave.nWidth * mmrgbasave.nHeight) >> 1) + 8; break;
            case 8: header.nTextureDataSize = (mmrgbasave.nWidth  * mmrgbasave.nHeight) + 8; break;
        }
    }

    //open the file
    FILE* file = fopen( pszFilename, "wb" );
    if( file == NULL ) return ReturnError("Failed to open file for output: ", pszFilename );

    //write out globalindex
    if( g_bEnableGlobalIndex )
    {
        GlobalIndexHeader gbix;
        memcpy( gbix.GBIX, "GBIX", 4 );
        gbix.nByteOffsetToNextTag = 8;
        gbix.nGlobalIndex = g_nGlobalIndex;
        unsigned long int zero = 0;
        if( fwrite( &gbix, 1, sizeof(GlobalIndexHeader), file ) < sizeof(GlobalIndexHeader) || fwrite( &zero, 1, sizeof(zero), file ) < sizeof(zero) )
        {
            fclose( file );
            return ReturnError("Write error on GBIX header: ", pszFilename );
        }

    }

    //write out file header
    unsigned long int nHeaderPosition = ftell( file ); //store the position of the header
    if( fwrite( &header, 1, sizeof(header), file ) < sizeof(header) )
    {
        fclose(file);
        return ReturnError("Write error on PVRT header: ", pszFilename );
    }


    //write out image
    int iMipMap = 0;
    if( bMipmaps )
    {
        if( mmrgbasave.nMipMaps <= 1 ) mmrgbasave.GenerateMipMaps();
        iMipMap = mmrgbasave.nMipMaps - 1;
        if( mmrgbasave.nMipMaps != mmrgbasave.nAlphaMipMaps ) mmrgbasave.GenerateAlphaMipMaps();

        const unsigned short int zero = 0;
        fwrite( &zero, 1, sizeof(zero), file );
    }


    int nTempWidth = bMipmaps ? 1 : mmrgbasave.nWidth;
    int nTempHeight = bMipmaps ? 1 : mmrgbasave.nHeight;
    while( iMipMap >= 0 )
    {
        if( nPaletteDepth == 0 )
        {
            //compute twiddle bit values
            unsigned long int mask, shift;
            if( bTwiddled ) ComputeMaskShift( nTempWidth, nTempHeight, mask, shift );

            //allocate a buffer to write into
            unsigned short int* pFileBuffer = (unsigned short int*)malloc( nTempWidth*nTempHeight*2 );
            memset( pFileBuffer, 0, nTempWidth*nTempHeight*2 );

            //write each texel into the buffer
            int iRead = 0, iAlphaRead = 0, iWrite = 0, iMax = nTempWidth * nTempHeight;
            int x = 0, y = 0;
            while( iWrite < iMax )
            {
                //get RGB & A
                unsigned char b = mmrgbasave.pRGB[iMipMap][iRead++];
                unsigned char g = mmrgbasave.pRGB[iMipMap][iRead++];
                unsigned char r = mmrgbasave.pRGB[iMipMap][iRead++];
                unsigned char a = (mmrgbasave.pAlpha && mmrgbasave.pAlpha[0]) ? mmrgbasave.pAlpha[iMipMap][iAlphaRead++]: g_nOpaqueAlpha;

                //calculate position and write computed texel
                int iPos = ( bTwiddled ) ? CalcUntwiddledPos( x, y, mask, shift ) : iWrite;
                ComputeTexel( x, y, &pFileBuffer[ iPos ], a, r, g, b, pSaveOptions->ColourFormat );

                //update position
                iWrite++;
                x++; if( x >= nTempWidth ) { x = 0; y++; }
            }

            //write the buffer to the file
            if( (int)fwrite( pFileBuffer, 1, iMax*2, file ) < (iMax*2) )
            {
                free( pFileBuffer );
                fclose( file );
                return ReturnError("Write error: ", pszFilename );
            }

            //clean up
            free( pFileBuffer );
        }
        else
        {
            //compute twiddle bit values
            unsigned long int mask, shift;
            ComputeMaskShift( nTempWidth, nTempHeight, mask, shift );

            //allocate a buffer to write into
            int iMax = (nTempWidth * nTempHeight);
            if( nPaletteDepth == 4 ) iMax >>= 1;
            unsigned char* pFileBuffer = (unsigned char*)malloc( iMax );
            memset( pFileBuffer, 0, iMax );

            //write each texel into the buffer
            int iRead = 0, iAlphaRead = 0, iWrite = 0;
            int x = 0, y = 0;
            if( nPaletteDepth == 4 )
            {
                while( iWrite < iMax )
                {
                    //get index
                    unsigned char index = mmrgbasave.pPaletteIndices[iMipMap][iRead++];
                    if( y & 1 ) { index <<= 4; iWrite++; } else index &= 0x0F;

                    //calculate position and write computed texel
                    int iPos = CalcUntwiddledPos( x, y, mask, shift );
                    iPos >>= 1;
                    pFileBuffer[iPos] |= index;

                    //update position
                    x++; if( x >= nTempWidth ) { x = 0; y++; }
                }
            }
            else
            {
                while( iWrite < iMax )
                {
                    //get index
                    unsigned char index = mmrgbasave.pPaletteIndices[iMipMap][iRead++];

                    //calculate position and write computed texel
                    pFileBuffer[ CalcUntwiddledPos( x, y, mask, shift ) ] = index;

                    //update position
                    iWrite++;
                    x++; if( x >= nTempWidth ) { x = 0; y++; }
                }
            }

            //write the buffer to the file
            if( (int)fwrite( pFileBuffer, 1, iMax, file ) < iMax )
            {
                free( pFileBuffer );
                fclose( file );
                return ReturnError("Write error: ", pszFilename );
            }

            //clean up
            free( pFileBuffer );
        }

        //advance values
        nTempWidth *= 2;
        nTempHeight *= 2;
        iMipMap--;
    }


    //if it's a stride texture, pad the rest of the file up to the next power of 2 if requested
    if( pSaveOptions->bPad && bStride )
    {
        int nExtra = (GetNearestPow2(mmrgbasave.nWidth) * GetNearestPow2(mmrgbasave.nHeight) * 2) - (mmrgbasave.nWidth*mmrgbasave.nHeight*2);
        void* pPadding = malloc( nExtra );
        memset( pPadding, 0, nExtra );
        fwrite( pPadding, 1, nExtra, file );
        free( pPadding );
    }

    //close the file
    fclose( file );

    //save palette
    if( nPaletteDepth != 0 )
    {
        //build palette file name
        char szPaletteFile[MAX_PATH+1];
        strcpy( szPaletteFile, pszFilename );
        ChangeFileExtension( szPaletteFile, "PVP" );

        //save the palette
        if( !SavePVRPalette( szPaletteFile, nPaletteDepth, icfPalette, mmrgbasave.Palette ) )
            DisplayStatusMessage( "Failed to write palette" );
    }

    return true;
}


#pragma pack( pop )
