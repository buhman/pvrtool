/*************************************************
 C Data Declaration Export
 
   Writes a C data declaration of a PVR file.
   It DOES NOT write out a declaration for
   "raw" RGB data or anything

  To Do

    * write out all file header info (+gbix) not just w & h
    * write out file header #defines info before main data

**************************************************/
#include <stdio.h>
#include "C.h"
#include "Util.h"
#include "PVR.h"
#include "VQF.h"

#define BYTES_PER_LINE 16


//////////////////////////////////////////////////////////////////////
// Write C formatted bytes into the given text file
//////////////////////////////////////////////////////////////////////
void WriteBytes( FILE* file, unsigned char* pData, int nCount )
{
    int nPosition = 0;
    for( int i = 0; i < nCount; i++ )
    {
        //write out data (put a tab in front if it's the first line)
        if( nPosition == 0 ) fprintf( file, "\t");
        fprintf( file, "0x%02X, ", pData[i] );

        //update position
        nPosition++;
        if( nPosition >= BYTES_PER_LINE && i < nCount )
        {
            fprintf( file, "\n" );
            nPosition = 0;
        }
    }
}


//////////////////////////////////////////////////////////////////////
// Writes a C file from the given in-memory PVR file
//////////////////////////////////////////////////////////////////////
bool WriteCFromPVR( const char* pszFilename, unsigned char* pPVR, int nSize )
{
    unsigned char* pPtr = pPVR;
    
    //open output file
    FILE* file = fopen( pszFilename, "wt" );
    if( file == NULL ) { free(pPVR); return ReturnError( "could not open file for output: ", pszFilename ); };

    //get a pointer to the filename without the extension or path
    char* pszTmp = strrchr( pszFilename, '/' );
    if( pszTmp == NULL ) pszTmp = strrchr( pszFilename, '\\' );
    if( pszTmp == NULL ) pszTmp = (char*)pszFilename; else pszTmp++;
    char* pszPathlessFilename = pszTmp;

    //write out file info etc.
    fprintf( file, "/********************\nPVR %s\n********************/\n\n", pszPathlessFilename );

    //get the filename without a path or extension
    char szRawFileName[32] = "";
    strncpy( szRawFileName, pszPathlessFilename, 32 );
    pszTmp = strrchr( szRawFileName, '.' );
    if( pszTmp ) *pszTmp = '\0';

    //write out variable declaration
    fprintf( file, "\n\n\nunsigned char p%s_pvrtex[] = {\n", szRawFileName );

    //see if we've got a GBIX or not
    if( pPtr[0] == 'G' && pPtr[1] == 'B' && pPtr[2] == 'I' && pPtr[3] == 'X' )
    {
        GlobalIndexHeader* pGBIX = (GlobalIndexHeader*)pPtr;
        int nHeaderSizeAndPadding = sizeof(GlobalIndexHeader) + (pGBIX->nByteOffsetToNextTag-4);
       
        fprintf( file, "\t/*global index header*/\n" );
        WriteBytes( file, pPtr, nHeaderSizeAndPadding );
        pPtr += nHeaderSizeAndPadding;
        fprintf( file, "\n" );
    }

    //write out file header
    fprintf( file, "\t/*file header*/\n" );
    WriteBytes( file, pPtr, sizeof(PVRHeader) );

    //see if there's a codebook
    PVRHeader* pHeader = (PVRHeader*)pPtr;
    int nTextureType = pHeader->nTextureType;
    bool bCodebook = false;
    switch( nTextureType & 0xFF00 )
    {
        case KM_TEXTURE_VQ:
        case KM_TEXTURE_VQ_MM:
        case KM_TEXTURE_SMALLVQ:
        case KM_TEXTURE_SMALLVQ_MM:
            bCodebook = true;
    }

    //move pointer over header
    pPtr += sizeof(PVRHeader);

    //write out codebook
    if( bCodebook )
    {
        fprintf( file, "\n\t/*codebook*/\n" );
        WriteBytes( file, pPtr, 256 * sizeof(VQFCodeBookEntry) );
        pPtr += (256 * sizeof(VQFCodeBookEntry));
    }

    //write out image data
    fprintf( file, "\n\t/*image*/\n" );
    WriteBytes( file, pPtr, (nSize - (pPtr-pPVR) ) );
    
    //write out end of declaration
    fprintf( file, "\n};\n\n" );

    //write out dimensions
    strupr( szRawFileName );
    fprintf( file, "\n\n#define %s_WIDTH %d\n", szRawFileName, pHeader->nWidth );
    fprintf( file, "#define %s_HEIGHT %d\n\n", szRawFileName, pHeader->nHeight );

    //clean up
    fclose( file );

    return true;
}




//////////////////////////////////////////////////////////////////////
// Saves a C file from the given mmrgba
//////////////////////////////////////////////////////////////////////
bool SaveC( const char* pszFilename, MMRGBA &mmrgba, SaveOptions* pSaveOptions )
{
    //create a temporary file name
    char* pszTempName = tmpnam( NULL );
    if( pszTempName == NULL ) return ReturnError( "Failed to create a temporary file for: ", pszFilename );

    //create the PVR file into this temporary file
    if( !SavePVR( pszTempName, mmrgba, pSaveOptions ) ) return false;

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

