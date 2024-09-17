#ifndef _C_H_
#define _C_H_
#pragma pack( push, 1 )

#include "Picture.h"

//externals
extern unsigned long int g_nGlobalIndex ;
extern bool g_bEnableGlobalIndex;

extern bool SaveC( const char* pszFilename, MMRGBA &mmrgba, SaveOptions* pSaveOptions );
extern bool WriteCFromPVR( const char* pszFilename, unsigned char* pPVR, int nSize );

#pragma pack( pop )
#endif //_PVR_H_
