/*************************************************
 Colour Functions
 
   This file contains functions to pack and unpack
   16-bit colours as well as palettised colours.
   
  Note: The YUV conversion method requires two
  texels worth of data, so the pointers are stored
  and only written to every other texel.

**************************************************/

#include "Util.h"
#include "Colour.h"


unsigned char g_nOpaqueAlpha = 0xFF;


//////////////////////////////////////////////////////////////////////
// Computes the correct 16-bit texel value for the given colour format
// it takes a pointer to the texel because the YUV conversion sets
// two texels at once.
//////////////////////////////////////////////////////////////////////
void ComputeTexel( int x, int y, unsigned short int* texel, unsigned char a, unsigned char r, unsigned char g, unsigned char b, ImageColourFormat icf )
{
    switch( icf )
    {
        case ICF_565:   *texel = MAKE_565( r, g, b ); break;
        case ICF_555:   *texel = MAKE_1555( 0, r, g, b ); break;
        case ICF_1555:  *texel = MAKE_1555( a, r, g, b ); break;
        case ICF_4444:  *texel = MAKE_4444( a, r, g, b ); break;
        case ICF_YUV422:
        {
            static unsigned short int* _texel;
            static unsigned char _r;
            static unsigned char _g;
            static unsigned char _b;

			if( !(x&1) ) //even pixel
			{
                _texel = texel;
                _r = r;
                _g = g;
                _b = b;
			}
			else //odd pixel
			{
                //compute each pixel's Y
			    unsigned Y0 = (unsigned)(0.299*_r + 0.587*_g + 0.114*_b);
                unsigned Y1 = (unsigned)(0.299*r + 0.587*g + 0.114*b);

                //average both pixel's rgb values
                r = ( r + _r ) / 2;
                g = ( g + _g ) / 2;
                b = ( b + _b ) / 2;

                //compute UV
				unsigned U = (unsigned)(128.0f - 0.14*r - 0.29*g + 0.43*b);
				unsigned V = (unsigned)(128.0f + 0.36*r - 0.29*g - 0.07*b);
                *_texel = (Y0<<8) | U;
                *texel = (Y1<<8) | V;
			}
			break;
        }
    }
}



//////////////////////////////////////////////////////////////////////
// Unpacks the 16-bit texel into ARGB values using the given format
// it takes pointers to the argb values instead of passing them by
// reference because two sets of rgb values are changed at once by
// the YUV conversion
//////////////////////////////////////////////////////////////////////
void UnpackTexel( int x, int y, unsigned short int texel, unsigned char* a, unsigned char* r, unsigned char* g, unsigned char* b, ImageColourFormat icf )
{
    switch( icf )
    {
        case ICF_565:
            if(a) *a = g_nOpaqueAlpha;
            *r = ( texel & 0xF800 ) >> 8;
            *g = ( texel & 0x07E0 ) >> 3;
            *b = ( texel & 0x001F ) << 3;
            break;

        case ICF_555:
            if(a) *a = g_nOpaqueAlpha;
            *r = (texel & 0x7C00) >> 7;
            *g = (texel & 0x03E0) >> 2;
            *b = (texel & 0x001F) << 3;
            break;

        case ICF_1555:
            if(a) *a = (texel & 0x8000) ? 0xFF : 0x00;
            *r = (texel & 0x7C00) >> 7;
            *g = (texel & 0x03E0) >> 2;
            *b = (texel & 0x001F) << 3;
            break;

        case ICF_4444:
            if(a) *a = ( texel & 0xF000 ) >> 8;
            *r = ( texel & 0x0F00 ) >> 4;
            *g = ( texel & 0x00F0 );
            *b = ( texel & 0x000F ) << 4;
            break;

        case ICF_YUV422:
        {
            if(a) *a = g_nOpaqueAlpha;
            static unsigned char* _r, * _g, * _b;
            static unsigned short int _texel;

            if( !(x&1) ) //even pixel
            {
                _texel = texel;
                _r = r;
                _g = g;
                _b = b;
            }
            else //odd pixel
            {
                //note: these must be declared as signed otherwise we have to spend
                //several days trying to get the YUV->RGB conversion to work and
                //wondering why it isn't. :-)
                signed int Y0 = ( _texel & 0xFF00 ) >> 8, U = ( _texel & 0x00FF );
                signed int Y1 = (  texel & 0xFF00 ) >> 8, V = (  texel & 0x00FF );

                *_r = Limit255(int(Y0 + 1.375*(V-128)));
                *_g = Limit255(int(Y0 - 0.6875*(V-128)-0.34375*(U-128)));
                *_b = Limit255(int(Y0 + 1.71875*(U-128)));

                *r =  Limit255(int(Y1 + 1.375*(V-128)));
                *g =  Limit255(int(Y1 - 0.6875*(V-128)-0.34375*(U-128)));
                *b =  Limit255(int(Y1 + 1.71875*(U-128)));
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////
// Unpacks the palettised texel index into ARGB values using the given 
// palette format.
//////////////////////////////////////////////////////////////////////
void UnpackPalettisedTexel( int x, int y, unsigned char indexbyte, unsigned char* a, unsigned char* r, unsigned char* g, unsigned char* b, ImageColourFormat icfPalette, int nPaletteDepth, void* pPalette )
{
    //handle 4bpp palette split
    if( nPaletteDepth == 4 )
    {
        if( y & 1 ) indexbyte = (indexbyte >> 4); else indexbyte = indexbyte & 0x0F;
    }

    //validate index
    if( indexbyte > ( 1 << nPaletteDepth ) )
    {
        if( a ) *a = g_nOpaqueAlpha;
        *r = *g = *b = 0;
        return;
    }

    //unpack palette value
    switch( icfPalette )
    {
        case ICF_1555:
        case ICF_565:
        case ICF_4444:
        {
            unsigned short int* pPal16 = (unsigned short int*)pPalette;
            UnpackTexel( x, y, pPal16[indexbyte], a, r, g, b, icfPalette );
            break;
        }

        case ICF_8888:
        {
            unsigned long int* pPal32 = (unsigned long int*)pPalette;
            if(a) *a = (unsigned char)(( pPal32[indexbyte] & 0xFF000000 ) >> 0x18);
            *r =       (unsigned char)(( pPal32[indexbyte] & 0x00FF0000 ) >> 0x10);
            *g =       (unsigned char)(( pPal32[indexbyte] & 0x0000FF00 ) >> 0x08);
            *b =       (unsigned char)(  pPal32[indexbyte] & 0x000000FF );
            break;
        }

        default:
            if( a ) *a = g_nOpaqueAlpha;
            *r = *g = *b = 0;
            break;
    }
}
