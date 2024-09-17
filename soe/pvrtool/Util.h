#ifndef _UTIL_H
#define _UTIL_H

//reference some of the GUI components if this is the GUI version
#ifdef _WINDOWS
    extern const char* szWindowTitle;
    extern HWND g_hWnd;
    extern HINSTANCE g_hInstance;
    extern BOOL g_bStretchMipmaps;
#endif


//external helper functions
extern void IndicateLongOperation( bool bTurnOn );
extern void ShowErrorMessage( const char* pszErrorMessageFormat, ... );
extern void DisplayStatusMessage( const char* pszMessageFormat, ... );
extern void BackupFile( const char* pszFilename );
extern bool ReturnError( const char* pszMessage, const char* pszFilename = NULL );
extern const char* GetFileExtension( const char* pszFilename );
extern const char* GetFileNameNoPath( const char* pszFilename );
extern void ByteSwap( unsigned short int& n );
extern void ByteSwap( unsigned long int& n );
extern void ByteSwap( signed short int& n );
extern void ByteSwap( signed long int& n );
extern unsigned char Limit255( int v );
extern int GetNearestPow2( int v );
extern void PrefixFileName( char* pszResult, const char* pszFilename, const char* pszPrefix );
extern void ChangeFileExtension( char* pszFilename, const char* pszExtension );
extern int Limit( int val, int min, int max );

#ifdef _WINDOWS
extern BOOL WritePrivateProfileInt( const char* pszAppName, const char* pszKeyName, int nValue, const char* pszINIFile );
extern BOOL WritePrivateProfileFloat( const char* pszAppName, const char* pszKeyName, float fValue, const char* pszINIFile );
#endif

//cheezy macros 
#define SWAP(x,y)   ((x)^=(y)^=(x)^=(y)) 


//memory leak checking:
#ifdef _DEBUG
    #define _CRTDBG_MAP_ALLOC
    
#include <crtdbg.h>
    void* operator new(size_t nSize, LPCSTR lpszFileName, int nLine);
    #define DEBUG_NEW new(__FILE__, __LINE__)
    void operator delete(void* p, LPCSTR lpszFileName, int nLine);
    #define new DEBUG_NEW
#endif




#endif //_UTIL_H
