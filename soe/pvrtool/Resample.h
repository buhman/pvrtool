#ifndef _RESAMPLE_H_
#define _RESAMPLE_H_

#include "Picture.h"

//texture resampling functions
extern void ResampleAlpha( unsigned char* pRes, unsigned char* pSrc, int nCurWidth, int nCurHeight );
extern void ResampleRGB( unsigned char* pRes, unsigned char* pSrc, int nCurWidth, int nCurHeight );
extern void ResamplePalette( unsigned char* pRes, unsigned char* pSrc, int nCurWidth, int nCurHeight, MMRGBAPAL* pPalette );

//texture resampling methods
enum ResampleMethod { Resample_2x2 };
extern ResampleMethod g_ResampleMethod;

#endif //_RESAMPLE_H_
