#ifndef _COLOUR_H_
#define _COLOUR_H_

//default opaque alpha value
extern unsigned char g_nOpaqueAlpha;

//internal representation of colour formats
enum ImageColourFormat { ICF_NONE, ICF_SMART, ICF_555, ICF_1555, ICF_4444, ICF_565, ICF_SMARTYUV, ICF_YUV422, ICF_8888, ICF_PALETTE4, ICF_PALETTE8 };

//colour format conversion
void ComputeTexel( int x, int y, unsigned short int* texel, unsigned char a, unsigned char r, unsigned char g, unsigned char b, ImageColourFormat icf );
void UnpackTexel( int x, int y, unsigned short int texel, unsigned char* a, unsigned char* r, unsigned char* g, unsigned char* b, ImageColourFormat icf );
void UnpackPalettisedTexel( int x, int y, unsigned char indexbyte, unsigned char* a, unsigned char* r, unsigned char* g, unsigned char* b, ImageColourFormat icfPalette, int nPaletteDepth, void* pPalette );

//16-bit colour packing macros
#define MAKE_565(r,g,b)    (unsigned short)(((unsigned short)(r)<<8)&0xF800 | ((unsigned short)(g)<<3)&0x07E0 | ((unsigned short)(b)>>3)&0x001F)
#define MAKE_4444(a,r,g,b) (unsigned short)(((unsigned short)(a)<<8)&0xF000 | ((unsigned short)(r)<<4)&0x0F00 | ((unsigned short)(g)   )&0x00F0 | ((unsigned short)(b)>>4)&0x000F)
#define MAKE_1555(a,r,g,b) (unsigned short)(((unsigned short)(a)<<8)&0x8000 | ((unsigned short)(r)<<7)&0x7C00 | ((unsigned short)(g)<<2)&0x03E0 | ((unsigned short)(b)>>3)&0x001F)
#define MAKE_8888(a,r,g,b) (unsigned long)( ((unsigned long)(a)<<24)&0xFF000000 | ((unsigned long)(r)<<16)&0x00FF0000 | ((unsigned long)(g)<<8)&0x0000FF00 | ((unsigned long)(b))&0x000000FF )


#endif //_COLOUR_H_
