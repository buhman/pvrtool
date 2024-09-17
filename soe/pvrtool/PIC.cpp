/*************************************************
 SoftImage PIC file loading

  Note: This filter has not been tested with
  all possible combinations of options

**************************************************/

#pragma pack( push, 1 )

#include <stdio.h>
#include <memory.h>
#include "PIC.h"
#include "Image.h"
#include "Util.h"


//////////////////////////////////////////////////////////////////////
// Loads the given file into the mmrgba object
//////////////////////////////////////////////////////////////////////
bool LoadPIC( const char* pszFilename, MMRGBA &mmrgba, unsigned long int dwFlags )
{
    /* load the image file into a buffer */

    //open the file
    FILE* file = fopen( pszFilename, "rb" );
    if( file == NULL ) return false;

    //load the file into a buffer
    fseek( file, 0, SEEK_END ); int nFileLength = ftell( file ); fseek( file, 0, SEEK_SET );
    unsigned char* pBuffer = (unsigned char*)malloc( nFileLength );
    if( (int)fread( pBuffer, 1, nFileLength, file ) < nFileLength ) { fclose(file); free(pBuffer); return false; };
    fclose(file);

    unsigned char* pPtr = pBuffer;


    /* read, translate and validate header */
    PICHeader* pHeader = (PICHeader*)pPtr;
    pPtr += sizeof(PICHeader);

    ByteSwap( pHeader->nWidth );
    ByteSwap( pHeader->nHeight );
    ByteSwap( pHeader->nFields );

    if( pHeader->magic != 0x34f68053 || memcmp( pHeader->PICT, "PICT", 4 ) != 0 )
    {
        ShowErrorMessage( "Invalid SoftImage PIC file" );
        free( pBuffer );
        return false;
    }
    if( pHeader->nFields != PIC_FIELD_FULLFRAME )
    {
        ShowErrorMessage( "Unsupported file type - Full Frame only" );
        free( pBuffer );
        return false;
    }


    /* count and store channels */
    PICChannelInfo* pChannels = (PICChannelInfo*)pPtr;
    pPtr += sizeof(PICChannelInfo);
    int nChannels = 0;
    while( pChannels[nChannels++].isChained == 1 ) pPtr += sizeof(PICChannelInfo);

    /* see if we've got an alpha channel */
    bool bAlpha = false;
    for( int iChannel = 0; iChannel < nChannels; iChannel++ ) if( pChannels[iChannel].channel & PIC_CHANNELCODE_ALPHA ) { bAlpha = true; break; }


    /* allocate the image */
    mmrgba.Init( MMINIT_ALLOCATE|MMINIT_RGB|(( bAlpha && (dwFlags & LPF_LOADALPHA) )?MMINIT_ALPHA:0), pHeader->nWidth, pHeader->nHeight );

    /* decompress */
	for( int y = 0; y < pHeader->nHeight; y++ )
	{
		for( int iChannel = 0; iChannel < nChannels; iChannel++ )
		{
            if( pChannels[iChannel].type & PIC_CHANNELTYPE_MIXED_RUN_LENGTH )
			{
                for( int x = 0; x < pHeader->nWidth; )
                {
                    if( *pPtr < 128 )
                    {
                        //one byte count
                        int nCount = (*pPtr++) + 1;
                        for( int iRun = 0; iRun < nCount; iRun++ )
                        {
                            int nChannel = pChannels[iChannel].channel;
                            if( nChannel & PIC_CHANNELCODE_RED )    mmrgba.pRGB[0][ ((x + (y*pHeader->nWidth)) * 3)+2 ] = *pPtr++;
                            if( nChannel & PIC_CHANNELCODE_GREEN )  mmrgba.pRGB[0][ ((x + (y*pHeader->nWidth)) * 3)+1 ] = *pPtr++;
                            if( nChannel & PIC_CHANNELCODE_BLUE )   mmrgba.pRGB[0][ ((x + (y*pHeader->nWidth)) * 3)   ] = *pPtr++;
                            if( nChannel & PIC_CHANNELCODE_ALPHA ) {
                              if( mmrgba.pAlpha ) mmrgba.pAlpha[0][   x + (y*pHeader->nWidth) ] = *pPtr++;
                              else pPtr++;
                            }
                            x++;
                        }
                    }
                    else
                    {
                        int nCount = 0;
                        if( *pPtr == 128 )
                        {
                            pPtr++;
                            nCount = *pPtr++;
                            nCount = (nCount*256) + *pPtr++;
                        }
                        else
                        {
                            nCount = (*pPtr++) - 127;
                        }

                        int iOff = 0;
                        for( int i = 0; i < nCount; i++ )
                        {
                            iOff = 0;
                            int nChannel = pChannels[iChannel].channel;
                            if( nChannel & PIC_CHANNELCODE_RED )    mmrgba.pRGB[0][ ((x + (y*pHeader->nWidth)) * 3)+2 ] = pPtr[iOff++];
                            if( nChannel & PIC_CHANNELCODE_GREEN )  mmrgba.pRGB[0][ ((x + (y*pHeader->nWidth)) * 3)+1 ] = pPtr[iOff++];
                            if( nChannel & PIC_CHANNELCODE_BLUE )   mmrgba.pRGB[0][ ((x + (y*pHeader->nWidth)) * 3)   ] = pPtr[iOff++];
                            if( nChannel & PIC_CHANNELCODE_ALPHA )  {
                              if( mmrgba.pAlpha ) mmrgba.pAlpha[0][   x + (y*pHeader->nWidth) ] = pPtr[iOff++];
                              else iOff++;
                            }
                            x++;
                        }
                        pPtr += iOff;
                    }
                }
			}
			else
			{
				for( int x = 0; x < pHeader->nWidth; x++)
				{
                    int nChannel = pChannels[iChannel].channel;
                    if( nChannel & PIC_CHANNELCODE_RED )    mmrgba.pRGB[0][ ((x + (y*pHeader->nWidth)) * 3)+2 ] = *pPtr++;
                    if( nChannel & PIC_CHANNELCODE_GREEN )  mmrgba.pRGB[0][ ((x + (y*pHeader->nWidth)) * 3)+1 ] = *pPtr++;
                    if( nChannel & PIC_CHANNELCODE_BLUE )   mmrgba.pRGB[0][ ((x + (y*pHeader->nWidth)) * 3)   ] = *pPtr++;
                    if( nChannel & PIC_CHANNELCODE_ALPHA )  {
                      if( mmrgba.pAlpha ) mmrgba.pAlpha[0][   x + (y*pHeader->nWidth) ] = *pPtr++;
                      else pPtr++;
                    }
				} 
			}
		}
	}

    //set description
    sprintf( mmrgba.szDescription, "SoftImage PIC %dx%d %s", pHeader->nWidth, pHeader->nHeight, pHeader->szComment );

    //free the image data and return
    free( pBuffer );
    return true;
}


#pragma pack( pop )
