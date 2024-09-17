/*************************************************
 VQ Compression Wrapper Object
 
   This class returns a CVQImage from a given
   CImage. It currently uses the VQ compression
   dll and some of it's member variables are
   strongly tied to the dll's enums.

**************************************************/

#include <stdio.h>
#include <string.h>
#include "Util.h"
#include "VQCompressor.h"

extern unsigned char g_nOpaqueAlpha;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CVQCompressor::CVQCompressor()
{
    m_bMipmap = false;
    m_bTolerateHigherFrequency = false;
    m_icf = ICF_SMART;
    m_nCodeBookSize = 256;
    m_Dither = VQSubtleDither;
    m_Metric = VQMetricRGB;
}

CVQCompressor::~CVQCompressor()
{

}



//////////////////////////////////////////////////////////////////////
// VQ Generation
//////////////////////////////////////////////////////////////////////
CVQImage* CVQCompressor::GenerateVQ( CImage* pImage )
{
    bool bTempAlpha = false;

    //make sure we've got an image
    if( pImage == NULL )
    {
        ShowErrorMessage( "Error: No image" );
        return NULL;
    }

    //make sure image is square
    if( pImage->GetWidth()  != pImage->GetHeight() )
    {
        ShowErrorMessage( "Error: Image must be square" );
        return NULL;
    }

    //make sure image is a size we can work with
    switch( pImage->GetWidth() )
    {
        case 8:
        case 16:
        case 32:
        case 64:
        case 128:
        case 256:
        case 512:
        case 1024:
            break;

//        case 2048:
//            DisplayStatusMessage( "Warning: Image size is > 1024 - this isn't compatible with hardware!" );
//            break;
//
        default:
            ShowErrorMessage( "Error: Image dimension must be a power of 2 between 8 and 1024" );
            return NULL;
    }

    //convert the image to 32 bit
    if( pImage->GetMMRGBA()->bPalette )
    {
        pImage->GetMMRGBA()->ConvertTo32Bit();
    }
    else
        if( pImage->GetRGB() == NULL ) { ShowErrorMessage( "Error: No image"); return NULL; }

    //get a VQ friendly version of the colour format
    int nColourFormat;
    ImageColourFormat icf = m_icf;
    if( m_icf == ICF_SMART || m_icf == ICF_SMARTYUV ) icf = pImage->GetMMRGBA()->GetBestColourFormat( m_icf );
    bool bAlpha = false;
    switch( icf )
    {
        default:
        case ICF_4444:  nColourFormat = FORMAT_4444; bAlpha = true; break;
        case ICF_565:   nColourFormat = FORMAT_565;  bAlpha = false; break;
        case ICF_555:   nColourFormat = FORMAT_555;  bAlpha = false; break;
        case ICF_1555:  nColourFormat = FORMAT_1555; bAlpha = true; break;
        case ICF_YUV422:nColourFormat = FORMAT_YUV;  bAlpha = false; break;
    }


    //make sure we've got a valid alpha channel if they want want
    unsigned char* pAlpha = pImage->GetAlpha();
    bool bReverseAlpha = false;
    if( ( bAlpha && pAlpha == NULL ) || ( !bAlpha && nColourFormat == FORMAT_4444 ) )
    {
        if( pAlpha == NULL )
        {
            pAlpha = (unsigned char*)malloc( pImage->GetWidth() * pImage->GetWidth() );
            memset( pAlpha, g_nOpaqueAlpha, pImage->GetWidth() * pImage->GetWidth() );
            bTempAlpha = true;
        }

        bAlpha = true;
    }
    else
    {
        if( g_nOpaqueAlpha == 0x00 ) bReverseAlpha = true;
    }

    //see if the supplied image has mipmaps if mipmaps are requested
    unsigned char* pInputRGB = pImage->GetRGB();
    unsigned char* pInputAlpha = pAlpha;
    bool bTempRGB = false;
    VQ_MIPMAP_MODES mipmapMode = VQ_NO_MIPMAP;
    if( m_bMipmap )
    {
        if( pImage->GetNumMipMaps() > 1 )
        {
            mipmapMode = VQ_SUPPLIED_MIPMAP;

            bTempRGB = true;
            int nTempBufferSize = 0, nTempWidth = pImage->GetWidth();
            while( nTempWidth > 0 ) { nTempBufferSize += (nTempWidth*nTempWidth); nTempWidth >>= 1; }
            nTempWidth = pImage->GetWidth();

            if( bTempAlpha ) { free( pInputAlpha );  pInputAlpha = NULL; }


            pInputRGB = (unsigned char*)malloc( nTempBufferSize * 3 );
            
            
            int i; unsigned char* pTmp = pInputRGB;
            for( i = 0; i < pImage->GetNumMipMaps(); i++ )
            {
                int nSize = (nTempWidth >> i) * (nTempWidth >> i) * 3;
                memcpy( pTmp, pImage->GetMMRGBA()->pRGB[i], nSize );
                pTmp += nSize;
            }

            if( pAlpha )
            {
                pInputAlpha = (unsigned char*)malloc( nTempBufferSize );
                if( bTempAlpha )
                    memset( pInputAlpha, g_nOpaqueAlpha, nTempBufferSize );
                else
                {
                    for( i = 0; i < pImage->GetNumMipMaps(); i++ )
                    {
                        int nSize = (nTempWidth >> i) * (nTempWidth >> i);
                        memcpy( pTmp, pImage->GetMMRGBA()->pAlpha[i], nSize );
                        pTmp += nSize;
                    }
                }
            }
        }
        else
            mipmapMode = VQ_GENERATE_MIPMAP;
    }

    //calculate how much space is needed to store the image
    float fErrorFound = 0.0f;
    VQ_COLOUR_METRIC Metric = m_Metric;
    if( m_bTolerateHigherFrequency ) Metric = VQ_COLOUR_METRIC( int(Metric)|VQMETRIC_FREQUENCY_FLAG );

    int nSize = VqCalc2( pInputRGB, pInputAlpha, NULL, true, pImage->GetWidth(), 0, mipmapMode, bAlpha, false, m_Dither, m_nCodeBookSize, nColourFormat, bReverseAlpha, Metric, 0, &fErrorFound );

    //perform processing
    if( nSize > 0 )
    {
        IndicateLongOperation( true );

        //allocate a buffer to hold the result
        unsigned char* pVQ = (unsigned char*)malloc( nSize );
        memset( pVQ, 0, nSize );

        //perform the calculations
        int nResult = VqCalc2( pInputRGB, pInputAlpha, pVQ, true, pImage->GetWidth(), 0, mipmapMode, bAlpha, false, m_Dither, m_nCodeBookSize, nColourFormat, bReverseAlpha, Metric, 0, &fErrorFound );

        //display overall error
        if( nResult >= 0 )
        {
            char szMessage[200]; sprintf( szMessage, "Done. %.03f average error", fErrorFound );
            DisplayStatusMessage( szMessage );
        }

        //turn off long operation indicator
        IndicateLongOperation( false );

        //remove temporary buffers
        if( bTempAlpha ) free( pInputAlpha );
        if( bTempRGB ) free( pInputRGB );

        //create a new CVQImage if it worked
        if( nResult >= 0 )
        {
            CVQImage* pVQImage = new CVQImage();
            pVQImage->SetVQ( pVQ, nSize, m_nCodeBookSize, pImage->GetWidth(), icf, m_bMipmap );
#ifdef _WINDOWS
            pVQImage->m_bChanged = true;
            pVQImage->UncompressVQ();
#endif
            pVQImage->m_icf = icf;
            return pVQImage;
        }
        else
            return NULL;

    }
    else
    {
        switch( nSize )
        {
            case VQ_OUTOFMEMORY:       ShowErrorMessage( "VQ Error: Out of memory" ); break;
            case VQ_INVALID_SIZE:      ShowErrorMessage( "VQ Error: Invalid size" ); break;
            case VQ_INVALID_PARAMETER: ShowErrorMessage( "VQ Error: Invalid parameter" ); break;
            default: ShowErrorMessage( "VQ Error: %d", nSize ); break;
        }
    }

    //remove temporary buffers
    if( bTempAlpha ) free( pInputAlpha );
    if( bTempRGB ) free( pInputRGB );

    return NULL;
}
