/*************************************************
 Utility Functions

   This file contains various misc. library
   functions

**************************************************/

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "util.h"

#ifdef _WINDOWS
#include <commctrl.h>
#include "WinPVR/resource.h"
#include "WinPVR/Log.h"
#endif


//////////////////////////////////////////////////////////////////////
// Limit function
//////////////////////////////////////////////////////////////////////
int Limit( int val, int min, int max )
{
    if( val > max )
        return max;
    if( val < min ) return min;
    return val;
}



//////////////////////////////////////////////////////////////////////
// Ensures the given value is between 0 and 255
//////////////////////////////////////////////////////////////////////
unsigned char Limit255( int v )
{
    if( v <= 0 ) return 0;
    else if( v >= 255 ) return 255;
    else return unsigned char(v);
}


//////////////////////////////////////////////////////////////////////
// Returns the next highest power of 2 for the given integer
//////////////////////////////////////////////////////////////////////
int GetNearestPow2( int v )
{
    int bit = 1;
    while( bit < v ) bit <<= 1;
    return bit;
}


//////////////////////////////////////////////////////////////////////
// Long operation indication (no implementation in non-gui)
//////////////////////////////////////////////////////////////////////
void IndicateLongOperation(bool bTurnOn)
{
#ifdef _WINDOWS
    SetCursor( LoadCursor( NULL, bTurnOn ? IDC_WAIT : IDC_ARROW ) );
#endif

}



//////////////////////////////////////////////////////////////////////
// Error message display
//////////////////////////////////////////////////////////////////////
void ShowErrorMessage( const char* pszErrorMessageFormat, ... )
{
    char szBuffer[4096];
    va_list params;
    va_start( params, pszErrorMessageFormat );
    vsprintf( szBuffer, pszErrorMessageFormat, params );
    va_end( params );

#ifdef _WINDOWS
    MessageBox( GetActiveWindow(), szBuffer, szWindowTitle, MB_ICONERROR );
    Log( szBuffer );

    //clear status bar
    HWND hWndActive = GetParent( GetActiveWindow() );
    if( hWndActive == NULL ) hWndActive = GetActiveWindow();
    SendDlgItemMessage( hWndActive, IDC_STATUSBAR, SB_SETTEXT, 0, (LPARAM)"" );
#else
    fprintf( stderr, "%s\n", szBuffer );
#endif

}



//////////////////////////////////////////////////////////////////////
// Status message display
//////////////////////////////////////////////////////////////////////
void DisplayStatusMessage( const char* pszMessageFormat, ... )
{
    char szBuffer[4096];
    va_list params;
    va_start( params, pszMessageFormat );
    vsprintf( szBuffer, pszMessageFormat, params );
    va_end( params );

#ifdef _WINDOWS
    HWND hWndActive = GetParent( GetActiveWindow() );
    if( hWndActive == NULL ) hWndActive = GetActiveWindow();
    SendDlgItemMessage( hWndActive, IDC_STATUSBAR, SB_SETTEXT, 0, (LPARAM)szBuffer );
    Log( szBuffer );

#else
    printf( "%s\n", szBuffer );
#endif

}



//////////////////////////////////////////////////////////////////////
// Error message display & return helper function - always returns false
//////////////////////////////////////////////////////////////////////
bool ReturnError( const char* pszMessage, const char* pszFilename /*NULL*/ )
{
    if( pszFilename == NULL )
    {
        ShowErrorMessage( pszMessage );
    }
    else
    {
        char* pszErrorMessage = (char*)malloc( strlen(pszMessage) + strlen(pszFilename) + 1 );
        strcpy( pszErrorMessage, pszMessage );
        strcat( pszErrorMessage, pszFilename );
        ShowErrorMessage( pszErrorMessage );
        free( pszErrorMessage );
    }
    return false;
}



//////////////////////////////////////////////////////////////////////
// Byte swapping functions for little & big endian file formats
//////////////////////////////////////////////////////////////////////
inline void Swap( unsigned char& b1, unsigned char& b2 )
{
    SWAP( b1, b2 );
/*
    static unsigned char tmp;
    tmp = b1;
    b1 = b2;
    b2 = tmp;
    */
}
inline void ByteSwap2( unsigned char* p )
{
    Swap( *(p), *(p+1) );
}
inline void ByteSwap4( unsigned char* p )
{
    Swap( *(p),   *(p+3) );
    Swap( *(p+1), *(p+2) );
}

void ByteSwap( unsigned short int& n )
{
    ByteSwap2( (unsigned char*)&n );
}
void ByteSwap( unsigned long int& n )
{
    ByteSwap4( (unsigned char*)&n );
}
void ByteSwap( signed short int& n )
{
    ByteSwap2( (unsigned char*)&n );
}
void ByteSwap( signed long int& n )
{
    ByteSwap4( (unsigned char*)&n );
}




//////////////////////////////////////////////////////////////////////
// Returns the file extension of the given file
//////////////////////////////////////////////////////////////////////
const char* GetFileExtension( const char* pszFilename )
{
    const char* pszEnd = &pszFilename[ strlen(pszFilename) ];
    if( pszFilename == NULL || *pszFilename == '\0' ) return pszEnd;
    const char* pszTemp = pszEnd - 1;
    while( pszTemp > pszFilename ) { pszTemp--; if( *pszTemp == '.' ) return &pszTemp[1]; }
    return pszEnd;
}

//////////////////////////////////////////////////////////////////////
// Changes the extension of the given file
//////////////////////////////////////////////////////////////////////
void ChangeFileExtension( char* pszFilename, const char* pszExtension )
{
    strcpy( (char*)GetFileExtension( pszFilename ), pszExtension );
}


//////////////////////////////////////////////////////////////////////
// Returns the filename of the given file
//////////////////////////////////////////////////////////////////////
const char* GetFileNameNoPath( const char* pszFilename )
{
    if( pszFilename == NULL || *pszFilename == '\0' ) return NULL;
    const char* pszTemp = strrchr(pszFilename,'\\' );
    if( pszTemp == NULL ) pszTemp = strrchr(pszFilename, '//');
    if( pszTemp ) return ++pszTemp; else return pszFilename;
}


//////////////////////////////////////////////////////////////////////
// Adds the given prefix to the given filename
//////////////////////////////////////////////////////////////////////
void PrefixFileName( char* pszResult, const char* pszFilename, const char* pszPrefix )
{
    //copy the filename
    strcpy( pszResult, pszFilename );

    //clip the string so it's only the path
    char* pszLastPath = strrchr(pszResult,'/');
    if( pszLastPath == NULL ) pszLastPath = strrchr(pszResult, '\\' );
    if( pszLastPath ) *++pszLastPath = '\0'; else *pszResult = '\0';

    //add the prefix and the raw filename
    strcat( pszResult, pszPrefix );
    strcat( pszResult, GetFileNameNoPath(pszFilename) );
}


//////////////////////////////////////////////////////////////////////
// Renames the given file to .bak
//////////////////////////////////////////////////////////////////////
void BackupFile( const char* pszFilename )
{
    //build backup filename
    char szBackupFilename[MAX_PATH];
    strcpy( szBackupFilename, pszFilename );
    strcpy( (char*)GetFileExtension(szBackupFilename), "bak" );

    //delete old backup
    remove( szBackupFilename );

    //move old file to backup
    rename( pszFilename, szBackupFilename );
}


#ifdef _WINDOWS

//////////////////////////////////////////////////////////////////////
// Additional WritePrivateProfileXXX functions
//////////////////////////////////////////////////////////////////////
BOOL WritePrivateProfileInt( const char* pszAppName, const char* pszKeyName, int nValue, const char* pszINIFile )
{
    char szTemp[100];
    wsprintf( szTemp, "%d", nValue );
    return WritePrivateProfileString( pszAppName, pszKeyName, szTemp, pszINIFile );
}

BOOL WritePrivateProfileFloat( const char* pszAppName, const char* pszKeyName, float fValue, const char* pszINIFile )
{
    char szTemp[100];
    sprintf( szTemp, "%.03f", fValue );
    return WritePrivateProfileString( pszAppName, pszKeyName, szTemp, pszINIFile );
}

#endif


//////////////////////////////////////////////////////////////////////
// Implementation of memory-leak tracking new & delete operators
//////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
    #ifdef new
        #undef new
    #endif

    void operator delete(void* pData, LPCSTR lpszFileName, int nLine )
    {
        ::operator delete(pData);
    }

    void* operator new(size_t nSize, LPCSTR lpszFileName, int nLine)
    {
        return ::operator new(nSize, _CLIENT_BLOCK, lpszFileName, nLine);
    }
#endif
