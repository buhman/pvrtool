#ifndef _PVR_H_
#define _PVR_H_
#pragma pack( push, 1 )

#include <stdint.h>
#include "Picture.h"

//externals
extern unsigned long int g_nGlobalIndex ;
extern bool g_bEnableGlobalIndex;


//category code
#define KM_TEXTURE_TWIDDLED	            (0x0100)
#define KM_TEXTURE_TWIDDLED_MM	        (0x0200)
#define KM_TEXTURE_VQ	                (0x0300)
#define KM_TEXTURE_VQ_MM	            (0x0400)
#define KM_TEXTURE_PALETTIZE4	        (0x0500)
#define KM_TEXTURE_PALETTIZE4_MM	    (0x0600)
#define KM_TEXTURE_PALETTIZE8	        (0x0700)
#define KM_TEXTURE_PALETTIZE8_MM	    (0x0800)
#define KM_TEXTURE_RECTANGLE	        (0x0900)
#define KM_TEXTURE_RECTANGLE_MM	        (0x0A00)	/* Reserved : Can't use. */
#define KM_TEXTURE_STRIDE	            (0x0B00)
#define KM_TEXTURE_STRIDE_MM	        (0x0C00)	/* Reserved : Can't use. */
#define KM_TEXTURE_TWIDDLED_RECTANGLE   (0x0D00)
#define KM_TEXTURE_BMP                  (0x0E00)	/* Converted to Twiddled */
#define KM_TEXTURE_BMP_MM               (0x0F00)	/* Converted to Twiddled MM */
#define KM_TEXTURE_SMALLVQ              (0x1000)
#define KM_TEXTURE_SMALLVQ_MM           (0x1100)


//Pixel format codes
#define KM_TEXTURE_ARGB1555             0x00000000   //Format consisting of one bit of alpha value and five bits of RGB values The alpha value indicates transparent when it is 0 and opaque when it is 1.
#define KM_TEXTURE_RGB565               0x00000001   //Format without alpha value and consisting of five bits of RB values and six bits of G value
#define KM_TEXTURE_ARGB4444             0x00000002   //Format consisting of four bits of alpha value and four bits of RGB values. The alpha value indicates completely transparent when it is 0x0 and completely opaque when it is 0xF.
#define KM_TEXTURE_YUV422               0x00000003   //YUV422 format
#define KM_TEXTURE_BUMP                 0x00000004   //Specifies a texture for bump mapping ARGB8888 color can be specified in Palettized format only. In this case, it is set for a palette by kmPaletteMode API instead of nTextureType.
#define KM_TEXTURE_RGB555               0x00000005 	 //for PCX compatible only.
#define KM_TEXTURE_YUV420               0x00000006 	 //for YUV converter


//other constants
#define MAX_GBIX                        0xFFFFFFEF   //from Ninja_GD.pdf pg. 99 "the numbers from 0xFFFFFFF0 to 0xFFFFFFFF is used by the system"


struct GlobalIndexHeader 
{
    uint8_t GBIX[4];
    uint32_t nByteOffsetToNextTag;
    uint32_t nGlobalIndex;
};
static_assert((sizeof (struct GlobalIndexHeader)) == 12);

struct PVRHeader 
{
    uint8_t PVRT[4];
    uint32_t nTextureDataSize;
    uint32_t nTextureType;
    uint16_t nWidth;
    uint16_t nHeight;
};
static_assert((sizeof (struct PVRHeader)) == 16);

struct PVRPaletteHeader //WARNING: structure is only a guess from looking at the output of PVRConv!!
{
    uint8_t PVPL[4];
    uint32_t nPaletteDataSize;
    uint32_t nType;
    uint16_t nUnknown;
    uint16_t nPaletteEntryCount;
};
static_assert((sizeof (struct PVRPaletteHeader)) == 16);

extern bool SavePVR( const char* pszFilename, MMRGBA &mmrgba, SaveOptions* pSaveOptions );
extern bool LoadPVR( const char* pszFilename, MMRGBA &mmrgba, unsigned long int dwFlags );

#pragma pack( pop )
#endif //_PVR_H_
