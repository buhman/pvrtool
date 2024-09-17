
#ifndef _PICTURE_H_
#define _PICTURE_H_

#include <stdlib.h>
#include "Colour.h"

#pragma pack( push, 1 )

//save options
struct SaveOptions
{
    ImageColourFormat ColourFormat;
    bool bTwiddled;
    bool bMipmaps;
    bool bPad;
    int nPaletteDepth;
};

struct MMRGBAPAL {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

#define MMINIT_RGB          (1)
#define MMINIT_ALPHA        (2)
#define MMINIT_MIPMAP       (4)
#define MMINIT_ALLOCATE     (8)
#define MMINIT_PALETTE      (16)

//wrapper class for RGBA Mipmap data
class MMRGBA 
{
public:
    MMRGBA() { pRGB = pAlpha = pPaletteIndices = NULL; nPaletteDepth = 0; nMipMaps = 0; nAlphaMipMaps = 0; nWidth = nHeight = 0; bPalette = false; icfOrig = icfOrigPalette = ICF_NONE; szDescription[0] = '\0'; }
    ~MMRGBA() { DeleteRGB(); DeleteAlpha(); DeletePalette(); }

    void Init( unsigned short int nFlagsint, int nWidth, int nHeight );

    void ReplaceWith( MMRGBA* other );
    void Copy( const MMRGBA& other );

    int CalcMipMapsFromWidth();
    ImageColourFormat GetBestColourFormat( ImageColourFormat icf );

    void AddAlpha();

    void Delete();
    void DeleteAllMipmaps();
    void DeleteRGB();
    void DeleteAlpha();
    void DeletePalette();

    void GenerateMipMaps();
    void GenerateAlphaMipMaps();

	void ConvertToPalettised( int nNewDepth );
    void ConvertTo32Bit();

    int nWidth, nHeight;
    int nMipMaps, nAlphaMipMaps;
    unsigned char** pPaletteIndices;
    unsigned char** pRGB;
    unsigned char** pAlpha;

    ImageColourFormat icfOrig;          //these are not the current settings, merely
    ImageColourFormat icfOrigPalette;   //what the original file's colour formats were

    bool bPalette;
    int nPaletteDepth;
    
    char szDescription[1024];
    MMRGBAPAL Palette[256];
};

//picture loading flags
#define LPF_LOADALPHA 1

//extern mmrgba load/save functions
extern bool LoadPicture( const char* pszFilename, MMRGBA& mmrgba, unsigned long int dwFlags = 0 );
extern bool SavePicture( const char* pszFilename, MMRGBA& mmrgba, const SaveOptions* const pSaveOptions );

#pragma pack( pop )

#endif //_PICTURE_H_
