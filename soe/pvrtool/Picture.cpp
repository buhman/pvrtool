/*************************************************
 Several picture related functions

  This file contains graphics utility functions,
  generic picture loading/saving routines and
  the implementation of the raw mipmapped RGBA
  wrapper class.


**************************************************/
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "stricmp.h"
#include "Picture.h"
#include "Resample.h"
#include "Util.h"
#include "PVR.h"
#include "VQF.h"
#include "C.h"
#include "PIC.h"
//#include "paintlib/paintlib.h"
#include "stb_image.h"

const char* g_pszSupportedFormats[] =
{
    //[filter]         [description]

    //internal texture
    "*.pvr",            "PVR texture",
    "*.vqf",            "VQF texture ",
    //internal picture formats
    "*.pic",            "PIC file (SoftImage)",
    //paintlib picture formats
    "*.bmp",            "BMP file (Windows bitmap)",
    "*.jpg;*.jpeg",     "JPEG file (joint picture experts group)",
    "*.pct;*.pict",     "PICT file (Mac picture)",
    "*.pcx",            "PCX file (Z-Soft paintbrush)",
    "*.png",            "PNG file (portable network graphics)",
    "*.tga",            "TGA file (Truevision targa)",
    "*.tif;*.tiff",     "TIFF file (tagged image file format)",
    //end of list...
    NULL, NULL,
};



//central picture decoder (not thread safe)
//CAnyPicDecoder paintlib;

//////////////////////////////////////////////////////////////////////
// Loads the given file into the mmrgba object using the paintlib library
//////////////////////////////////////////////////////////////////////
/*
bool LoadPictureUsingPaintLib( const char* pszFilename, MMRGBA& mmrgba, unsigned long int dwFlags )
{
    //load image
    CAnyBmp bmp;
    
    try
    {
        //try to load the image file
        paintlib.MakeBmpFromFile( pszFilename, &bmp, 0 );
    }
    catch( CTextException e )
    {
        //failed - display message (although don't do error box for files we don't recognise)
        if( e.GetCode() != 8 ) //8 = unknown file type
            ShowErrorMessage( "Error decoding %s: %s", pszFilename, (const char *)e );
        return false;
    }

    //only load the alpha if it was requested
    bool bAlpha = (bmp.HasAlpha() == TRUE);
    bool bPalette = (bmp.GetPalette() != NULL);
    if( (dwFlags & LPF_LOADALPHA) == 0 && bAlpha ) { bmp.SetHasAlpha(false); bAlpha = false; }

    //initialise the mmrgba object
    int nWidth = bmp.GetWidth(), nHeight = bmp.GetHeight();
    mmrgba.Init( MMINIT_RGB|(bAlpha?MMINIT_ALPHA:0)|(bPalette?MMINIT_PALETTE:0)|MMINIT_ALLOCATE, nWidth, nHeight );

    if( bPalette )
    {
        //copy it over
        unsigned char* pPaletteIndices = mmrgba.pPaletteIndices[0];
        BYTE** pLineArray = bmp.GetLineArray();
        for( int y = 0; y < nHeight; y++ )
        {
            memcpy( &pPaletteIndices[ y * nWidth  ], pLineArray[y], nWidth );
        }

        //copy over palette
        unsigned char* pBmpPalette = (unsigned char*)bmp.GetPalette();
        for( int i = 0; i < 256; i++ )
        {
            mmrgba.Palette[i].b = *pBmpPalette++;
            mmrgba.Palette[i].g = *pBmpPalette++;
            mmrgba.Palette[i].r = *pBmpPalette++;
            mmrgba.Palette[i].a = *pBmpPalette++;
        }
    }
    else
    {
        //copy it over
        int iWrite = 0, iAWrite = 0;
        unsigned char* pRGB = mmrgba.pRGB[0];
        unsigned char* pAlpha = bAlpha ? mmrgba.pAlpha[0] : NULL;
        BYTE** pLineArray = bmp.GetLineArray();
        for( int y = 0; y < nHeight; y++ )
        {
            BYTE* pLine = pLineArray[y];
            for( int x = 0; x < nWidth; x++ )
            {
                pRGB[iWrite++] = pLine[x*4+RGBA_BLUE];
                pRGB[iWrite++] = pLine[x*4+RGBA_GREEN];
                pRGB[iWrite++] = pLine[x*4+RGBA_RED];
                if(bAlpha) pAlpha[iAWrite++] = pLine[x*4+RGBA_ALPHA];
            }
        }
    }


    //set description
    sprintf( mmrgba.szDescription, "%s file %dx%d %dbpp", GetFileExtension(pszFilename), mmrgba.nWidth, mmrgba.nHeight, bmp.GetBitsPerPixel() );
    return true;
}
*/

bool LoadPicture_stb_image(const char * filename, MMRGBA& mmrgba, unsigned long int dwFlags)
{
  bool use_alpha = (dwFlags & LPF_LOADALPHA) != 0;
  int request_components = use_alpha ? 4 : 3;
  int components;
  int width;
  int height;
  unsigned char * data = stbi_load(filename, &width, &height, &components, request_components);
  if (!data) {
    return false;
  }
  assert(components == 4 || components == 3);

  unsigned short int flags = 0
    | MMINIT_RGB
    | ((components == 4) ? MMINIT_ALPHA : 0)
    | MMINIT_ALLOCATE
    ;

  mmrgba.Init(flags, width, height);

  unsigned char * rgb = *mmrgba.pRGB;
  unsigned char * alpha = NULL;
  if (components == 4) alpha = *mmrgba.pAlpha;

  for (int i = 0; i < width * height; i++) {
    rgb[i * 3 + 0] = data[i * 4 + 2];
    rgb[i * 3 + 1] = data[i * 4 + 1];
    rgb[i * 3 + 2] = data[i * 4 + 0];
    if (components == 4) {
      alpha[i] = data[i * 4 + 3];
    }
  }

  mmrgba.bPalette = false;

  return true;
}

//////////////////////////////////////////////////////////////////////
// Loads the given file into the mmrgba object
//////////////////////////////////////////////////////////////////////
bool LoadPicture( const char* pszFilename, MMRGBA& mmrgba, unsigned long int dwFlags /*0*/ )
{
    //convert file name to lower case and get a pointer to the extension
    const char* pszExtension = GetFileExtension( pszFilename );
    if( pszExtension == NULL ) pszExtension = pszFilename;
    
    //set defaults
    mmrgba.Delete();

    //determine which file format function to use
/*  (these are superceded by paintlib)
    if( stricmp( pszExtension, "tga" ) == 0 ) return LoadTGA( pszFilename, mmrgba, dwFlags );
    if( stricmp( pszExtension, "bmp" ) == 0 ) return LoadBMP( pszFilename, mmrgba, dwFlags );
    if( stricmp( pszExtension, "tif" ) == 0 ) return LoadTIF( pszFilename, mmrgba, dwFlags ); if( stricmp( pszExtension, "tiff" ) == 0 ) return LoadTIF( pszFilename, mmrgba, dwFlags );
*/

    //try internal formats first
    if( stricmp( pszExtension, "pvr" ) == 0 ) return LoadPVR( pszFilename, mmrgba, dwFlags );
    if( stricmp( pszExtension, "vqf" ) == 0 ) return LoadVQF( pszFilename, mmrgba, dwFlags );
    if( stricmp( pszExtension, "pic" ) == 0 ) return LoadPIC( pszFilename, mmrgba, dwFlags );
    //FIXME: new formats go here...

    //see if the paintlib can handle it
    //if( LoadPictureUsingPaintLib( pszFilename, mmrgba, dwFlags ) ) return true;

    if (LoadPicture_stb_image(pszFilename, mmrgba, dwFlags)) return true;

    //unsupported file type
    return false;
}


//////////////////////////////////////////////////////////////////////
// Saves the given mmrgba object into the given file
//////////////////////////////////////////////////////////////////////
bool SavePicture( const char* pszFilename, MMRGBA& mmrgba, const SaveOptions* const pSaveOptions )
{
    //validate mmrgba
    if( mmrgba.pRGB == NULL && mmrgba.bPalette == false )
    {
        ShowErrorMessage( "No image. Save failed" );
        return false;
    }

    //if the user requests the 'clever' colour format, change it to the best one
    SaveOptions options = *pSaveOptions;
    if( options.ColourFormat == ICF_SMART || options.ColourFormat == ICF_SMARTYUV ) options.ColourFormat = mmrgba.GetBestColourFormat( options.ColourFormat );

    //determine which file format function to use
    const char* pszExtension = GetFileExtension(pszFilename);
    if( stricmp( pszExtension, "pvr" ) == 0 ) { BackupFile(pszFilename); return SavePVR( pszFilename, mmrgba, &options ); }
    if( stricmp( pszExtension, "c" ) == 0 || stricmp( pszExtension, "h" ) == 0 || stricmp( pszExtension, "cc" ) == 0 || stricmp( pszExtension, "cpp" ) == 0 || stricmp( pszExtension, "cxx" ) == 0 || stricmp( pszExtension, "hh" ) == 0 || stricmp( pszExtension, "hpp" ) == 0 || stricmp( pszExtension, "hxx" ) == 0 ) { BackupFile(pszFilename); return SaveC( pszFilename, mmrgba, &options ); }
    //FIXME: new formats go here...

    //unsupported file type
    return false;
}




/******************************************************************************************************************/

//////////////////////////////////////////////////////////////////////
// Initialises the structure based on the supplied flags
//////////////////////////////////////////////////////////////////////
void MMRGBA::Init( unsigned short int nFlags, int _nWidth, int _nHeight )
{
    //delete existing one
    Delete();

    //store size
    nWidth = _nWidth;
    nHeight = _nHeight;
    
    //get palette option
    bPalette = (nFlags & MMINIT_PALETTE) == MMINIT_PALETTE;

    //initialise
    if( bPalette )
    {
        //allocate new array
        nMipMaps = (nFlags & MMINIT_MIPMAP) ? CalcMipMapsFromWidth() : 1;
        pPaletteIndices = (unsigned char**)malloc( sizeof(unsigned char*) * nMipMaps );

        memset( Palette, 0, sizeof(MMRGBAPAL) * 256 );

        //allocate the pointers too, if requested
        for( int i = 0; i < nMipMaps; i++ )
        {
            if( nFlags & MMINIT_ALLOCATE )
            {
                int nSize = (nWidth >> i) * (nHeight >> i);
                pPaletteIndices[i] = (unsigned char*)malloc( nSize );
                memset( pPaletteIndices[i], 0, nSize );
            }
            else
            {
                pPaletteIndices[i] = NULL;
            }
        }

        //set image colour formats
        icfOrig = ICF_PALETTE8;
        icfOrigPalette = ICF_8888;
        nPaletteDepth = 8;
    }
    else
    {
        if( nFlags & MMINIT_RGB )
        {
            //allocate a new array
            nMipMaps = (nFlags & MMINIT_MIPMAP) ? CalcMipMapsFromWidth() : 1;
            pRGB = (unsigned char**)malloc( sizeof(unsigned char*) * nMipMaps );
            memset( pRGB, 0, sizeof(unsigned char*) * nMipMaps );
        
            //allocate the pointers too, if requested
            if( nFlags & MMINIT_ALLOCATE )
            {
                for( int i = 0; i < nMipMaps; i++ )
                {
                    int nSize = (nWidth >> i) * (nHeight >> i) * 3;
                    pRGB[i] = (unsigned char*)malloc( nSize );
                    memset( pRGB[i], 0, nSize );
                }
            }
        }

        if( nFlags & MMINIT_ALPHA )
        {
            //allocate a new array
            nAlphaMipMaps = (nFlags & MMINIT_MIPMAP) ? CalcMipMapsFromWidth() : 1;
            pAlpha = (unsigned char**)malloc( sizeof(unsigned char*) * nAlphaMipMaps );
            memset( pAlpha, 0, sizeof(unsigned char*) * nAlphaMipMaps );

            //allocate the pointers too, if requested
            if( nFlags & MMINIT_ALLOCATE )
            {
                for( int i = 0; i < nAlphaMipMaps; i++ )
                {
                    int nSize = (nWidth >> i) * (nHeight >> i);
                    pAlpha[i] = (unsigned char*)malloc( nSize );
                    memset( pAlpha[i], 0, nSize );
                }
            }
        }

        //set image colour formats
        icfOrig = ICF_8888;
        icfOrigPalette = ICF_NONE;   
    }
}

//////////////////////////////////////////////////////////////////////
// Adds an alpha channel
//////////////////////////////////////////////////////////////////////
void MMRGBA::AddAlpha()
{
    //only create alpha if there is RGB data
    if( pRGB == NULL || nWidth == 0 || nHeight == 0 ) return;

    //delete existing
    DeleteAlpha();

    //Set mipmap count
    nAlphaMipMaps = nMipMaps;

    //allocate a new array
    pAlpha = (unsigned char**)malloc( sizeof(unsigned char*) * nAlphaMipMaps );
    memset( pAlpha, 0, sizeof(unsigned char*) * nAlphaMipMaps );

    //allocate the pointers too
    for( int i = 0; i < nAlphaMipMaps; i++ )
    {
        int nSize = (nWidth >> i) * (nHeight >> i);
        pAlpha[i] = (unsigned char*)malloc( nSize );
        memset( pAlpha[i], 0, nSize );
    }
}



//////////////////////////////////////////////////////////////////////
// Calcualte the number of mipmaps from the width
//////////////////////////////////////////////////////////////////////
int MMRGBA::CalcMipMapsFromWidth()
{
    int nMMCount = 0;
    int nTempWidth = nWidth;
    while( nTempWidth )
    {
        nMMCount++;
        nTempWidth /= 2;
    }
    return nMMCount;
}

//////////////////////////////////////////////////////////////////////
// Deletes all content from the MMRGBA object
//////////////////////////////////////////////////////////////////////
void MMRGBA::Delete()
{
    DeleteRGB();
    DeleteAlpha();
    DeletePalette();
}


//////////////////////////////////////////////////////////////////////
// Deletes the RGB data for all mipmap levels
//////////////////////////////////////////////////////////////////////
void MMRGBA::DeleteRGB()
{
    if( pRGB )
    {
        for( int i = 0; i < nMipMaps; i++ ) if( pRGB[i] ) free( pRGB[i] );
        free( pRGB );
    }
    pRGB = NULL;
}


//////////////////////////////////////////////////////////////////////
// Deletes the alpha data for all mipmap levels
//////////////////////////////////////////////////////////////////////
void MMRGBA::DeleteAlpha()
{
    if( pAlpha )
    {
        for( int i = 0; i < nAlphaMipMaps; i++ ) if( pAlpha[i] ) free( pAlpha[i] );
        free( pAlpha );
    }
    pAlpha = NULL;
}

//////////////////////////////////////////////////////////////////////
// Deletes the palette data for all mipmap levels
//////////////////////////////////////////////////////////////////////
void MMRGBA::DeletePalette()
{
    if( pPaletteIndices )
    {
        for( int i = 0; i < nMipMaps; i++ ) if( pPaletteIndices[i] ) free( pPaletteIndices[i] );
        free( pPaletteIndices );
    }
    pPaletteIndices = NULL;
    bPalette = false;

}

//////////////////////////////////////////////////////////////////////
// Generate all mipmaps for the RGB data
//////////////////////////////////////////////////////////////////////
void MMRGBA::GenerateMipMaps()
{
    if( (pRGB == NULL && pPaletteIndices == NULL) || nWidth != nHeight  ) return;

    if( bPalette )
    {
        //store current image
        unsigned char* pPaletteIndicesImage = pPaletteIndices[0];

        //delete existing images
        for( int i = 1; i < nMipMaps; i++ ) if( pPaletteIndices[i] ) free( pPaletteIndices[i] );
        free( pPaletteIndices );

        //create new array
        nMipMaps = CalcMipMapsFromWidth();
        pPaletteIndices = (unsigned char**)malloc( sizeof(unsigned char*) * nMipMaps );
        memset( pPaletteIndices, 0, sizeof(unsigned char*) * nMipMaps );
        pPaletteIndices[0] = pPaletteIndicesImage;

        //generate mipmaps
        for( int i = 1; i < nMipMaps; i++ )
        {
            pPaletteIndices[i] = (unsigned char*)malloc( (nWidth >> i) * (nHeight >> i) );
            ResamplePalette( pPaletteIndices[i], pPaletteIndices[i-1], nWidth >> (i-1), nHeight >> (i-1), Palette );
        }
    }
    else
    {
        //store current RGB image
        unsigned char* pRGBImage = pRGB[0];

        //delete existing mipmaps
        for( int i = 1; i < nMipMaps; i++ ) if( pRGB[i] ) free( pRGB[i] );
        free( pRGB );

        //create a new RGB array
        nMipMaps = CalcMipMapsFromWidth();
        pRGB = (unsigned char**)malloc( sizeof(unsigned char*) * nMipMaps );
        memset( pRGB, 0, sizeof(unsigned char*) * nMipMaps );
        pRGB[0] = pRGBImage;

        //generate mipmaps
        for( int i = 1; i < nMipMaps; i++ )
        {
            pRGB[i] = (unsigned char*)malloc( (nWidth >> i) * (nHeight >> i) * 3 );
            ResampleRGB( pRGB[i], pRGB[i-1], nWidth >> (i-1), nHeight >> (i-1) );
        }
    }
}


//////////////////////////////////////////////////////////////////////
// Generate all mipmaps for the alpha data
//////////////////////////////////////////////////////////////////////
void MMRGBA::GenerateAlphaMipMaps()
{
    if( pAlpha == NULL || pAlpha[0] == NULL || nWidth != nHeight ) return;

    //store current Alpha image
    unsigned char* pAlphaImage = pAlpha[0];

    //delete existing mipmaps
    for( int i = 1; i < nAlphaMipMaps; i++ ) if( pAlpha[i] ) free( pAlpha[i] );
    free( pAlpha );

    //create a new Alpha array
    nAlphaMipMaps = CalcMipMapsFromWidth();
    pAlpha = (unsigned char**)malloc( sizeof(unsigned char*) * nAlphaMipMaps );
    memset( pAlpha, 0, sizeof(unsigned char*) * nAlphaMipMaps );
    pAlpha[0] = pAlphaImage;

    //generate mipmaps
    for( int i = 1; i < nAlphaMipMaps; i++ )
    {
        int nTempWidth = nWidth >> i, nTempHeight = nHeight >> i;
        pAlpha[i] = (unsigned char*)malloc( (nWidth >> i) * (nHeight >> i) );
        ResampleAlpha( pAlpha[i], pAlpha[i-1], nWidth >> (i-1), nHeight >> (i-1) );
    }
}
    

//////////////////////////////////////////////////////////////////////
// Deletes all mipmaps except the 100% x 100% one
//////////////////////////////////////////////////////////////////////
void MMRGBA::DeleteAllMipmaps()
{
    if( pRGB )
    {
        unsigned char* pRGBTemp = pRGB[0];
        for( int i = 1; i < nMipMaps; i++ ) free( pRGB[i] );
        free( pRGB );
        pRGB = (unsigned char**)malloc( sizeof(unsigned char*) );
        pRGB[0] = pRGBTemp;
        nMipMaps = 1;
    }

    if( pAlpha )
    {
        unsigned char* pAlphaTemp = pAlpha[0];
        for( int i = 1; i < nAlphaMipMaps; i++ ) free( pAlpha[i] );
        free( pAlpha );
        pAlpha = (unsigned char**)malloc( sizeof(unsigned char*) );
        pAlpha[0] = pAlphaTemp;
        nAlphaMipMaps = 1;
    }

    if( pPaletteIndices )
    {
        unsigned char* pPaletteTemp = pPaletteIndices[0];
        for( int i = 1; i < nMipMaps; i++ ) free( pPaletteIndices[i] );
        free( pPaletteIndices );
        pPaletteIndices = (unsigned char**)malloc( sizeof(unsigned char*) );
        pPaletteIndices[0] = pPaletteTemp;
        nMipMaps = 1;
    }
}


//////////////////////////////////////////////////////////////////////
// Returns the best image colour format for the current image
//////////////////////////////////////////////////////////////////////
ImageColourFormat MMRGBA::GetBestColourFormat( ImageColourFormat icf )
{
    if( icf != ICF_SMART && icf != ICF_SMARTYUV ) return icf;

    if( bPalette )
    {       
        //if the palette index goes above 15, assume it's an 8 bit palette (fixme: count palette references instead?)
        for( int iMipMap = 0; iMipMap < nMipMaps; iMipMap++ )
        {
            int nMax = (nWidth >> iMipMap) * (nHeight >> iMipMap);
            for( int i = 0; i < nMax; i++ ) if( pPaletteIndices[iMipMap][i] & 0xF0 ) return ICF_PALETTE8;
        }
        
        return ICF_PALETTE4;
    }
    else
    {
        //if there is an alpha channel...
        if( pAlpha && pAlpha[0] )
        {
            //get the first byte of the alpha channel
            unsigned char aFirst = pAlpha[0][0];
        
            //check it against all others
            unsigned char aSecond; 
            bool bGotSecond = false;
            for( int i = 1; i < nWidth * nHeight; i++ )
            {
                unsigned char a = pAlpha[0][i];

                //if this byte isn't the same as the first...
                if( a != aFirst )
                {
                    if( !bGotSecond )
                    {
                        //store the second alpha value
                        aSecond = a;
                        bGotSecond = true;
                    }
                    else
                    {
                        //there are three different alpha values so far - use 4444
                        if( a != aSecond ) return ICF_4444;
                    }
                }
            }

            //if we didn't get a second alpha, there was a plain alpha channel - use 565 or YUV as it's probably a mistake
            if( bGotSecond == false ) return (icf == ICF_SMARTYUV) ? ICF_YUV422 : ICF_565;

            //otherwise, we've only got two alpha values - use 1 bit alpha (1555)
            return ICF_1555;
        }
        else //no alpha - use 565 or YUV
            return (icf == ICF_SMARTYUV) ? ICF_YUV422 : ICF_565;
    }
}


//////////////////////////////////////////////////////////////////////
// Steals the images from the given source
//////////////////////////////////////////////////////////////////////
void MMRGBA::ReplaceWith( MMRGBA* other )
{
    //delete existing image
    Delete();

    //copy current one over
    *this = *other;

    //erase other one
    *other = MMRGBA();
}

//////////////////////////////////////////////////////////////////////
// Creates a duplicate of the given source
//////////////////////////////////////////////////////////////////////
void MMRGBA::Copy( const MMRGBA& other )
{
    //delete existing image
    Delete();

    //get image options
    bool bMipMaps = ( other.nMipMaps > 1);
    bool bAlpha = (other.pAlpha != NULL);

    //initialise the new image set
    Init( MMINIT_RGB|MMINIT_ALLOCATE|((other.bPalette)?MMINIT_PALETTE:0)|(bMipMaps?MMINIT_MIPMAP:0)|(bAlpha?MMINIT_ALPHA:0), other.nWidth, other.nHeight );

    //copy everything over
    for( int iMipMap = 0; iMipMap < nMipMaps; iMipMap++ )
    {
        int nSize = (nWidth >> iMipMap) * (nHeight >> iMipMap);

        if( bPalette )
        {
            memcpy( pPaletteIndices[iMipMap], other.pPaletteIndices[iMipMap], nSize );
        }
        else
        {
            memcpy( pRGB[iMipMap], other.pRGB[iMipMap], nSize * 3 );
            if( bAlpha ) memcpy( pAlpha[iMipMap], other.pAlpha[iMipMap], nSize );
        }
    }

    //copy palette etc
    if( bPalette ) memcpy( Palette, other.Palette, 256 * sizeof(MMRGBAPAL) );
    nPaletteDepth = other.nPaletteDepth;
    icfOrig = other.icfOrig;
    icfOrigPalette = other.icfOrigPalette;
    strcpy( szDescription, other.szDescription );
}



//////////////////////////////////////////////////////////////////////
// Converts the MMRGBA object from paletteised to 32bit RGBA
//////////////////////////////////////////////////////////////////////
void MMRGBA::ConvertTo32Bit()
{
    if( bPalette == false ) return;

    DisplayStatusMessage( "Converting image to 32bpp..." );

    //back up the current image into a temporary object
    MMRGBA other;
    other.ReplaceWith( this );
    strcpy( szDescription, other.szDescription );

    //get image options
    bool bMipMaps = ( other.nMipMaps > 1);
    bool bAlpha = (other.pAlpha != NULL);

    //initialise the new image
    Init( MMINIT_RGB|MMINIT_ALLOCATE|(bMipMaps?MMINIT_MIPMAP:0)|(bAlpha?MMINIT_ALPHA:0), other.nWidth, other.nHeight );

    //apply palette
    for( int iMipMap = 0; iMipMap < nMipMaps; iMipMap++ )
    {
        //build 32-bit image using palette
        int nSize = (other.nWidth >> iMipMap) * (other.nHeight >> iMipMap);
        unsigned char* pRGBWorkspace = pRGB[iMipMap];
        unsigned char* pAlphaWorkspace = bAlpha ? pAlpha[iMipMap] : NULL;
        for( int i2 = 0; i2 < nSize; i2++ )
        {
            pRGBWorkspace[(i2*3)+2] = other.Palette[ other.pPaletteIndices[iMipMap][i2] ].r;
            pRGBWorkspace[(i2*3)+1] = other.Palette[ other.pPaletteIndices[iMipMap][i2] ].g;
            pRGBWorkspace[(i2*3)  ] = other.Palette[ other.pPaletteIndices[iMipMap][i2] ].b;
            if( bAlpha ) pAlphaWorkspace[i2] = other.Palette[ other.pPaletteIndices[iMipMap][i2] ].a;               
        }
    }
    nPaletteDepth = 0;
}

//////////////////////////////////////////////////////////////////////
// Converts the MMRGBA object to the requested palette depth
//////////////////////////////////////////////////////////////////////
void MMRGBA::ConvertToPalettised(int nNewDepth)
{
    if( bPalette && nNewDepth == nPaletteDepth ) return;

    ShowErrorMessage( "This is not implemented yet: FIXME!" );
}
