/*************************************************
 RGB Buffer resampling functions

  This file contains the resampling functions.
  They are accessible through the ResampleRGB
  and ResampleAlpha functions. Select the
  algorithm required with g_ResampleMethod.

  To Do
  
    * Add better mipmap resampling methods than
      just 2x2

**************************************************/

#include "Resample.h"
#include "Picture.h"
#include "Util.h"

ResampleMethod g_ResampleMethod = Resample_2x2;

//////////////////////////////////////////////////////////////////////
// Simple 2x2 Mipmap resampling functions
//////////////////////////////////////////////////////////////////////
void ResampleRGB2x2( unsigned char* pRes, unsigned char* pSrc, int nCurWidth, int nCurHeight )
{
    int nNewWidth = nCurWidth / 2, nNewHeight = nCurHeight / 2;
    for( int y = 0; y < nCurHeight; y += 2 )
    {
        for( int x = 0; x < nCurWidth; x += 2 )
        {
            int i1, i2, i3, i4;
            i1 = ((x + (y * nCurWidth)) * 3);
            i2 = ((((x+1)%nCurWidth) + (y * nCurWidth)) * 3);
            i3 = ((x + ( ((y+1)%nCurHeight) * nCurWidth)) * 3);
            i4 = ((((x+1)%nCurWidth) + (((y+1)%nCurHeight) * nCurWidth)) * 3);

            unsigned char r = (pSrc[i1] + pSrc[i2] + pSrc[i3] + pSrc[i4]) / 4;
            unsigned char g = (pSrc[i1+1] + pSrc[i2+1] + pSrc[i3+1] + pSrc[i4+1]) / 4;
            unsigned char b = (pSrc[i1+2] + pSrc[i2+2] + pSrc[i3+2] + pSrc[i4+2]) / 4;

            int iWrite = ((x/2) + ((y/2) * nNewWidth ) ) * 3;
            pRes[iWrite++] = r;
            pRes[iWrite++] = g;
            pRes[iWrite++] = b;       
        }
    }
}
              
void ResampleAlpha2x2( unsigned char* pRes, unsigned char* pSrc, int nCurWidth, int nCurHeight )
{
    int nNewWidth = nCurWidth / 2, nNewHeight = nCurHeight / 2;
    for( int y = 0; y < nCurHeight; y += 2 )
    {
        for( int x = 0; x < nCurWidth; x += 2 )
        {
            int i1, i2, i3, i4;
            i1 = (x + (y * nCurWidth));
            i2 = (((x+1)%nCurWidth) + (y * nCurWidth));
            i3 = (x + ( ((y+1)%nCurHeight) * nCurWidth));
            i4 = (((x+1)%nCurWidth) + (((y+1)%nCurHeight) * nCurWidth));

            unsigned char a = ( pSrc[i1] + pSrc[i2] + pSrc[i3] + pSrc[i4] ) / 4;

            int iWrite = ((x/2) + ((y/2) * nNewWidth ) );
            pRes[iWrite++] = a;
        }
    }
}

void ResamplePalette2x2( unsigned char* pRes, unsigned char* pSrc, int nCurWidth, int nCurHeight, MMRGBAPAL* pPalette )
{
    /*

  fixme: this 2x2 sample did not produce very nice results, so I've switched to a cheapo pixel resize

    int nNewWidth = nCurWidth / 2, nNewHeight = nCurHeight / 2;
    for( int y = 0; y < nCurHeight; y += 2 )
    {
        for( int x = 0; x < nCurWidth; x += 2 )
        {
            //get index of each entry
            int i1 = pSrc[(x + (y * nCurWidth))];
            int i2 = pSrc[(((x+1)%nCurWidth) + (y * nCurWidth))];
            int i3 = pSrc[(x + ( ((y+1)%nCurHeight) * nCurWidth))];
            int i4 = pSrc[(((x+1)%nCurWidth) + (((y+1)%nCurHeight) * nCurWidth))];

            //calculate average colour
            int r = (int(pPalette[i1].r) + int(pPalette[i2].r) + int(pPalette[i3].r) + int(pPalette[i4].r)) / 4;
            int g = (int(pPalette[i1].g) + int(pPalette[i2].g) + int(pPalette[i3].g) + int(pPalette[i4].g)) / 4;
            int b = (int(pPalette[i1].b) + int(pPalette[i2].b) + int(pPalette[i3].b) + int(pPalette[i4].b)) / 4;
            int a = (int(pPalette[i1].a) + int(pPalette[i2].a) + int(pPalette[i3].a) + int(pPalette[i4].a)) / 4;

            //find the closest match
            unsigned char i = GetClosestMatchingPaletteEntry( pPalette, r, g, b, a );

            //write it out
            pRes[ ((x/2) + ((y/2) * nNewWidth ) ) ] = i;
        }
    }
    */

    int nNewWidth = nCurWidth / 2, nNewHeight = nCurHeight / 2;
    for( int y = 0; y < nCurHeight; y += 2 )
    {
        for( int x = 0; x < nCurWidth; x += 2 )
        {
            unsigned char i = pSrc[(x + (y * nCurWidth))];
            pRes[ ((x/2) + ((y/2) * nNewWidth )) ] = i;
        }
    }

}


//////////////////////////////////////////////////////////////////////
// Algorithm independent resampling functions
//////////////////////////////////////////////////////////////////////
void ResampleAlpha( unsigned char* pRes, unsigned char* pSrc, int nCurWidth, int nCurHeight )
{
    switch( g_ResampleMethod )
    {
        case Resample_2x2: ResampleAlpha2x2( pRes, pSrc, nCurWidth, nCurHeight ); break;
    }
}

void ResampleRGB( unsigned char* pRes, unsigned char* pSrc, int nCurWidth, int nCurHeight )
{
    switch( g_ResampleMethod )
    {
        case Resample_2x2: ResampleRGB2x2( pRes, pSrc, nCurWidth, nCurHeight ); break;
    }
}

void ResamplePalette( unsigned char* pRes, unsigned char* pSrc, int nCurWidth, int nCurHeight, MMRGBAPAL* pPalette )
{
    switch( g_ResampleMethod )
    {
        case Resample_2x2: ResamplePalette2x2( pRes, pSrc, nCurWidth, nCurHeight, pPalette ); break;
    }
}


