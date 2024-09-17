/*************************************************
 VQ-compressed image class
 
   This class inherits from the CImage class and
   manages the VQ compressed data. These objects
   are created by CVQCompressor


**************************************************/
#include <stdio.h>
#include <string.h>
#include "stricmp.h"
#include "Picture.h"
#include "Util.h"
#include "VQImage.h"
#include "PVR.h"
#include "VQF.h"
#include "Twiddle.h"
#include "C.h"

#ifdef _WINDOWS
    extern HWND g_hWnd;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CVQImage::CVQImage()
{
    m_pVQ = NULL;
    m_nVQSize = 0;
    m_icf = ICF_NONE;
    m_nVQCodebookSize = 0;
    m_nVQWidth = 0;
    m_icfVQ = ICF_NONE;
    m_bVQMipmap = false;
}

CVQImage::~CVQImage()
{
    SetVQ( NULL, 0, 0, 0, ICF_NONE, false );
}


#ifdef _WINDOWS



//////////////////////////////////////////////////////////////////////
// VQ Decompression
//////////////////////////////////////////////////////////////////////
bool CVQImage::UncompressVQ()
{
    /* get ready for decompression */
    if( m_pVQ == NULL ) return false;
    unsigned char* pVQ = m_pVQ;

    //see whether we've got an alpha channel or not
    bool bAlpha = ( m_icfVQ == ICF_1555 || m_icfVQ == ICF_4444 );

    //allocate the RGB buffer
    m_mmrgba.DeleteRGB();
    m_mmrgba.DeleteAlpha();
    m_mmrgba.Init( MMINIT_ALLOCATE|MMINIT_RGB| (bAlpha?MMINIT_ALPHA:0) |(m_bVQMipmap?MMINIT_MIPMAP:0), m_nVQWidth, m_nVQWidth );

    int nNumMipMaps = m_mmrgba.nMipMaps;

#ifdef _WINDOWS
    m_mmrgbaAligned.DeleteRGB();
    m_mmrgbaAligned.DeleteAlpha();
    m_mmrgbaAligned.Init( MMINIT_RGB| (bAlpha?MMINIT_ALPHA:0) |(m_bVQMipmap?MMINIT_MIPMAP:0), m_nVQWidth, m_nVQWidth );
#endif

    //get the codebook
    VQFCodeBookEntry* pCodeBook = (VQFCodeBookEntry*)pVQ;
    pVQ += sizeof(VQFCodeBookEntry) * m_nVQCodebookSize;

    //unpack image
    int iMipMap = m_bVQMipmap ? m_mmrgba.nMipMaps - 1 : 0;
    int nTempWidth = m_bVQMipmap ? 1 : m_mmrgba.nWidth;
    int nTempHeight = nTempWidth;
    while( iMipMap >= 0 )
    {
        unsigned char* pRGB = m_mmrgba.pRGB[iMipMap];
        unsigned char* pAlpha = m_mmrgba.pAlpha ? m_mmrgba.pAlpha[iMipMap] : NULL;

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
                UnpackTexel( 0, 0, pCodeBook[ pVQ[iPos] ].Texel[0], ( pAlpha && bAlpha ) ? &pAlpha[0] : NULL, &pRGB[2], &pRGB[1], &pRGB[0], ICF_565 );
            }
            else
            {
                //unpack the twiddled 2x2 block
                int xoff = 0, yoff = 0;
                int iLinear[] = { 0, 2, 1, 3 }; //this is needed so that YUV can be unpacked properly
                for( int iTexel = 0; iTexel < 4; iTexel++ )
                {
                    //get the texel
                    unsigned short int Texel = pCodeBook[ pVQ[iPos] ].Texel[iLinear[iTexel]];

                    //determine where to write
                    int iAlphaWrite = (x + xoff + ( (y + yoff) * nTempWidth));
                    int iWrite = iAlphaWrite * 3;

                    //unpack and write the valies into the buffer
                    unsigned char *pa, *pr, *pg, *pb;
                    pa = ( pAlpha && bAlpha ) ? &pAlpha[ iAlphaWrite ] : NULL;
                    pb = &pRGB[iWrite++];
                    pg = &pRGB[iWrite++];
                    pr = &pRGB[iWrite++];
                    UnpackTexel( x + xoff, y + yoff, Texel, pa, pr, pg, pb, m_icfVQ );

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

#ifdef _WINDOWS
        m_mmrgbaAligned.pRGB[iMipMap] = AlignBitmap( pRGB, nTempWidth, nTempWidth, 3 );
        if( pAlpha ) m_mmrgbaAligned.pAlpha[iMipMap] = AlignBitmap( pAlpha, nTempWidth, nTempWidth, 1 );
#endif

        //move pointer over the mipmap we just unpacked
        pVQ += nMax;

        //update values
        iMipMap--;
        nTempWidth *= 2;
        nTempHeight *= 2;
    }

    return true;
}
#endif



//////////////////////////////////////////////////////////////////////
// Modifies the image's VQ image data
//////////////////////////////////////////////////////////////////////
void CVQImage::SetVQ( unsigned char *pNewVQ, int nVQSize, int nCodebookSize, int nWidth, ImageColourFormat icf, bool bMipmap )
{
    if( m_pVQ ) free( m_pVQ );

    m_pVQ = pNewVQ;
    m_nVQSize = nVQSize;
    m_nVQWidth = nWidth;
    m_nVQCodebookSize = nCodebookSize;
    m_icfVQ = icf;
    m_bVQMipmap = bMipmap;
}





//////////////////////////////////////////////////////////////////////
// File Export
//////////////////////////////////////////////////////////////////////
#ifdef _WINDOWS
bool CVQImage::ExportFile()
#else
bool CVQImage::ExportFile( const char* s_szFilename )
#endif
{
    /* make sure we've got something to export */
    if( m_pVQ == NULL )
    {
        ShowErrorMessage( "The compressed image hasn't been created yet - nothing to export" );
        return false;
    }

#ifdef _WINDOWS
    /* get the file name */

    //prepare open file dialog
    static char s_szFilename[MAX_PATH] = "";
    OPENFILENAME ofn;
    ZeroMemory( &ofn, sizeof(ofn) );
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrTitle = "Export";
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = "PVR Texture (.pvr)\0*.pvr\0VQ Texture (.vqf)\0*.vqf\0C PVR File (.c)\0*.c\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = s_szFilename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_ENABLESIZING|OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "pvr";

    //display open file dialog
    if( GetSaveFileName( &ofn ) == false ) return false;
#endif

    //create backup
    BackupFile( s_szFilename );

    /* determine how to write it out */
    const char* pszExtension = GetFileExtension( s_szFilename );
    if( stricmp( pszExtension, "VQF" ) == 0 )      { if( !SaveAsVQF( s_szFilename ) ) return ReturnError( "Writing the VQF failed" ); }
    else if( stricmp( pszExtension, "PVR" ) == 0 ) { if( !SaveAsPVR( s_szFilename ) ) return ReturnError( "Writing the PVR failed" ); }
    else if( stricmp( pszExtension, "C" ) == 0 || stricmp( pszExtension, "h" ) == 0 || stricmp( pszExtension, "cc" ) == 0 || stricmp( pszExtension, "cpp" ) == 0 || stricmp( pszExtension, "cxx" ) == 0 || stricmp( pszExtension, "hh" ) == 0 || stricmp( pszExtension, "hpp" ) == 0 || stricmp( pszExtension, "hxx" ) == 0 ) { if( !SaveAsC( s_szFilename ) )   return ReturnError( "Writing the C-PVR failed" ); }
    else
        ShowErrorMessage( "Could not determine file format from file extension" );

#ifdef _WINDOWS
    m_bChanged = false;
#endif


    return true;
}



//////////////////////////////////////////////////////////////////////
// Dumps the VQ data into the given file
//////////////////////////////////////////////////////////////////////
bool CVQImage::SaveAsVQF( const char* pszFilename )
{
    //build header
    VQFHeader header;
    memset( &header, 0, sizeof(header) );
    memcpy( header.PV, "PV", 2 );

    switch( m_icfVQ )
    {
        case ICF_555:    header.nMapType = VQF_MAPTYPE_555; break;
        case ICF_565:    header.nMapType = VQF_MAPTYPE_565; break;
        case ICF_1555:   header.nMapType = VQF_MAPTYPE_1555; break;
        case ICF_4444:   header.nMapType = VQF_MAPTYPE_4444; break;
        case ICF_YUV422: header.nMapType = VQF_MAPTYPE_YUV422; break;
        default:         return false;
    }
    if( m_bVQMipmap ) header.nMapType |= VQF_MAPTYPE_MIPMAPPED;
    
    switch( m_nVQWidth )
    {
        case 8:     header.nTextureSize = 4; break;
        case 16:    header.nTextureSize = 5; break;
        case 32:    header.nTextureSize = 0; break;
        case 64:    header.nTextureSize = 1; break;
        case 128:   header.nTextureSize = 2; break;
        case 256:   header.nTextureSize = 3; break;
        case 512:   header.nTextureSize = 6; break;
        case 1024:  header.nTextureSize = 7; break;
        default:    return false;
    }

    switch( m_nVQCodebookSize )
    {
        case 8:     header.nCodeBookSize = 0; break;
        case 16:    header.nCodeBookSize = 1; break;
        case 32:    header.nCodeBookSize = 2; break;
        case 64:    header.nCodeBookSize = 3; break;
        case 128:   header.nCodeBookSize = 4; break;
        case 256:   header.nCodeBookSize = 5; break;
        default:    return false;
    }

    //open the file
    FILE* file = fopen( pszFilename, "wb" );
    if( file == NULL ) return false;

    //dump the header and vqf into it
    if( fwrite( &header, 1, sizeof(header), file ) < sizeof(header) || fwrite( m_pVQ, 1, m_nVQSize, file ) < (size_t)m_nVQSize )
    {
        fclose( file );
        return false;
    }

    //close it and return
    fclose( file );
    return true;  
}


//////////////////////////////////////////////////////////////////////
// Delete's the image's VQ data
//////////////////////////////////////////////////////////////////////
void CVQImage::Delete()
{
    SetVQ( NULL, 0, 0, 0, ICF_NONE, false );
    CImage::Delete();
}



bool CVQImage::SaveAsPVR(const char *pszFilename)
{
    //build header
    PVRHeader header;
    memcpy( header.PVRT, "PVRT", 4 );
    header.nHeight = header.nWidth = m_nVQWidth;
    header.nTextureDataSize = m_nVQSize + 8;
    header.nTextureType = 0;
    switch( m_icfVQ )
    {
        case ICF_YUV422:header.nTextureType |= KM_TEXTURE_YUV422; break;
        case ICF_4444:  header.nTextureType |= KM_TEXTURE_ARGB4444; break;
        case ICF_555:   header.nTextureType |= KM_TEXTURE_ARGB1555; break; 
        case ICF_1555:  header.nTextureType |= KM_TEXTURE_ARGB1555; break;
        default:
        case ICF_565:   header.nTextureType |= KM_TEXTURE_RGB565; break;
    }
    if( m_nVQCodebookSize == 256 )
        header.nTextureType |= ( m_bVQMipmap ? KM_TEXTURE_VQ_MM : KM_TEXTURE_VQ );
    else
        header.nTextureType |= ( m_bVQMipmap ? KM_TEXTURE_SMALLVQ_MM : KM_TEXTURE_SMALLVQ );


    //open the file
    FILE* file = fopen( pszFilename, "wb" );
    if( file == NULL ) return false;

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
            return false;
        }

    }

    //write out file header
    if( fwrite( &header, 1, sizeof(header), file ) < sizeof(header) )
    {
        fclose(file);
        return false;
    }
    
    //write out image data
    if( fwrite( m_pVQ, 1, m_nVQSize, file ) < (size_t)m_nVQSize )
    {
        fclose(file);
        return false;
    }

    //close the file and clean up
    fclose( file );
    return true;
}


bool CVQImage::SaveAsC( const char* pszFilename )
{
    //create a temporary file name
    char* pszTempName = tmpnam( NULL );
    if( pszTempName == NULL ) return ReturnError( "Failed to create a temporary file for: ", pszFilename );

    //create the PVR file into this temporary file
    if( !SaveAsPVR( pszTempName ) ) return false;

    //open the PVR file
    FILE* pvrfile = fopen( pszTempName, "rb" );
    if( pvrfile == NULL ) { remove( pszTempName ); return ReturnError( "Could not open tempoary file" ); }

    //load it into a buffer and delete the temporary file
    fseek( pvrfile, 0, SEEK_END ); int nFileLength = ftell( pvrfile ); fseek( pvrfile, 0, SEEK_SET );
    unsigned char* pPVR = (unsigned char*)malloc( nFileLength );
    if( (int)fread( pPVR, 1, nFileLength, pvrfile ) < nFileLength ) { fclose(pvrfile); free(pPVR); remove( pszTempName ); return ReturnError( "Unexpected end of temporary file" ); };
    fclose(pvrfile);
    remove( pszTempName );

    //write out the file
    bool bResult = WriteCFromPVR( pszFilename, pPVR, nFileLength );

    //clean up and return
    free( pPVR );
    return bResult;
}
