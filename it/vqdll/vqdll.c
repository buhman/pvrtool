#include <windows.h>

#include "vqcalc.h"

#define MyDllExport    __declspec( dllexport )
#include "Vqdll.h"


static HINSTANCE GlobalhInstance;

int WINAPI DllMain ( HINSTANCE hInstance, DWORD fdwReason, PVOID pvReserved)
{

	GlobalhInstance = hInstance;
	return TRUE;
}





/*
// The VQ compressor. See header file (vqdll.h) for more details.
*/
MyDllExport int VqCalc2(void*	InputArrayRGB,
						void*	InputArrayAlpha,
						void*	OutputMemory,

						int		BGROrder,		/*if data is ordered as BGR not RGB*/						 
						int		nWidth,			/*Square textures, power of 2 sizes*/	
						int		Reserved0, 		/*Reserved. Set it to 0 */

						int		bMipMap,		/*Generate MIP map. RECOMMENDED!*/
						int		bAlphaOn,		/*Is Alpha supplied?*/

						int		bIncludeHeader,	/*include the 12 byte header?*/

			VQ_DITHER_TYPES		DitherLevel,	/* Level of dithering required */

						int		nNumCodes,		/*Maximum codes requested*/
						int		nColourFormat,	/*see formats above		 */

						int		bInvertAlpha,	/*To support Old PVR1 files that had odd alpha */

			VQ_COLOUR_METRIC 	Metric,			/*how to estimate colour differences*/
						int 	Reserved1,		/*Reserved. Set it to 0 */

						float	*fErrorFound)
{

	/*
	// Call the internal function
	*/
	return CreateVq(InputArrayRGB, 
					InputArrayAlpha, 
					OutputMemory, 

					BGROrder, 
					nWidth,
					Reserved0,
					bMipMap,

					bAlphaOn,
					bIncludeHeader,

					DitherLevel,
					
					nNumCodes,
					nColourFormat,
										
					bInvertAlpha,

					Metric,
					Reserved1,

					fErrorFound);
}




/*
// The OBSOLETE function interface. Kept ***ONLY*** for backward compatibility
// 
//		Note: This version always generates a info header of 12 bytes
// 		preceding the VQ data.
//		To upload in video memory a VQ file created with this function,
//		this header has to be removed.
*/
MyDllExport int VqCalc(	void*	InputArrayRGB,
								void*	InputArrayAlpha,
								void*	OutputMemory, 
								int		nWidth,
								int		bMipMap,
								int		bAlphaOn,
								int		bDither,
								int		nNumCodes,
								int		nColourFormat,
								int		DummyA,
								int		DummyB,
								float	DummyC[3],
								float	DummyD[4],
								int		WeightMode,
								int		bInvertAlpha,
								float	DummyF,
								float	*fErrorFound)
{
	VQ_DITHER_TYPES		DitherLevel;

	/*
	// Setup the dithering/error diffusion mode
	*/
	if(bDither)
	{
		DitherLevel = VQSubtleDither;
	}
	else
	{
		DitherLevel = VQNoDither;
	}

	/*
	// This is only an approximation to what we used to have
	*/
	if(WeightMode)
	{
	 	WeightMode = VQMetricRGB;
	}
	else
	{
		WeightMode = VQMetricWeighted;
	}

	return CreateVq(InputArrayRGB, 
					InputArrayAlpha, 
					OutputMemory, 

					1, 			/*Old function assumed BGR order*/
					nWidth,
					0,
					bMipMap,

					bAlphaOn,
					1,			/*The old function ALWAYS created the header*/
					DitherLevel,
					
					nNumCodes,
					nColourFormat,
					bInvertAlpha,

					WeightMode,			
					0,	/*Reserved values*/		

					fErrorFound);
}


/*
// VERSION information. Added at the request of Sega Europe
*/

MyDllExport void VqGetVersionInfoString( char* pszVersionInfoString, const char* pszKey )
{
	WORD* pwLang = NULL;
	UINT uLen;
    char szFilename[MAX_PATH+1];
	DWORD dw, dwSize;
	LPVOID pVoid;

	/*
	// get the filename
	*/
	//GetModuleFileName( NULL, szFilename, MAX_PATH );
	GetModuleFileName(GlobalhInstance, szFilename, MAX_PATH );
   
	/*
	//load the version info into a block
	*/
    dwSize = GetFileVersionInfoSize( szFilename, &dw );
    pVoid = GlobalAlloc( GPTR, dwSize );
    GetFileVersionInfo( szFilename, 0, dwSize, pVoid );

    /*
	//read the translation value
	*/
    if( VerQueryValue( pVoid, "\\VarFileInfo\\Translation", (void**)&pwLang, &uLen ) )
    {
        char szKey[128];
		char* pszFileVersion = "";

		/*
		// build query key
		*/
        wsprintf( szKey, "\\StringFileInfo\\%08X\\%s", (pwLang[0] << 16 | pwLang[1]), 
						 pszKey );

        /*
		// read fileversion and copy it into the buffer
		*/
		if( VerQueryValue( pVoid, szKey, (void**)&pszFileVersion, &uLen ) )
		{
			strcpy( pszVersionInfoString, pszFileVersion );
		}
    }
    GlobalFree( pVoid );
}



/*
// END OF FILE
*/

