#ifndef _VQF_H_
#define _VQF_H_
#pragma pack( push, 1 )

#include "Picture.h"

#define VQF_MAPTYPE_555                 6
#define VQF_MAPTYPE_565                 7
#define VQF_MAPTYPE_1555                8
#define VQF_MAPTYPE_4444                9
#define VQF_MAPTYPE_YUV422              10
#define VQF_MAPTYPE_MIPMAPPED           64
#define VQF_MAPTYPE_555_MIPMAPPED       (VQF_MAPTYPE_555|VQF_MAPTYPE_MIPMAPPED)
#define VQF_MAPTYPE_565_MIPMAPPED       (VQF_MAPTYPE_565|VQF_MAPTYPE_MIPMAPPED)
#define VQF_MAPTYPE_1555_MIPMAPPED      (VQF_MAPTYPE_1555|VQF_MAPTYPE_MIPMAPPED)
#define VQF_MAPTYPE_4444_MIPMAPPED      (VQF_MAPTYPE_4444|VQF_MAPTYPE_MIPMAPPED)
#define VQF_MAPTYPE_YUV422_MIPMAPPED    (VQF_MAPTYPE_YUV422|VQF_MAPTYPE_MIPMAPPED)

struct VQFHeader {
    unsigned char PV[2];
    unsigned char nMapType;
    unsigned char nTextureSize;
    unsigned char _reserved4;
    unsigned char nCodeBookSize;
    unsigned char _reserved6;
    unsigned char _reserved7;
    unsigned char _reserved8;
    unsigned char _reserved9;
    unsigned char _reserved10;
    unsigned char _reserved11;
};

struct VQFCodeBookEntry
{
    unsigned short int Texel[4];
};



extern bool LoadVQF( const char* pszFilename, MMRGBA &mmrgba, unsigned long int dwFlags );

unsigned char* VQF2PVR( unsigned char* pVQFFile, int& nWidth, int& nCodebookSize, int& nPVRImageType );

extern int GetWidthFromTextureSizeCode( unsigned char nTextureSizeCode );
extern unsigned char GetTextureSizeCodeFromWidth( int nWidth );
extern int GetCodebookSizeFromCode( unsigned char nCodeBookSizeCode );
extern unsigned char GetCodeFromCodebookSize( int nCodeBookSize );


#pragma pack( pop )
#endif //_VQF_H_
