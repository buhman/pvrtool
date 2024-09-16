/*
// FILE: 		vqdll.h
//
// DECRIPTION: 	Prototype definitions for vqdll.dll, the dynamic link library
//		for PowerVR VQ texture compression.
//
//		Please use the first function interface:
//		The second is there only for compatibility with old applications
//
// HISTORY:		Modified 18 November 1999, Simon Fenney
//
//				Created  1998  Julian Hodgson
//
//
//	$Log: vqdll.h,v $
 * Revision 1.4  2000/01/10  15:42:29  sjf
 * Added "Vq" to the name of the info function.
 *
 * Revision 1.3  2000/01/10  15:19:11  sjf
 * Added function to return version info at request of Sega Europe.
 *
 * Revision 1.2  2000/01/06  15:18:22  sjf
 * Improved the comments in the file.
 *
//
//
*/


/*
//
//Usage of DLL:
//-------------
//
//The Dll should be called twice if you don't know how much memory is needed for the
//output file, once with OutputMemory passed NULL, upon which the function will return
//with the amount of memory needed.
//The second time just pass the pointer for the compressed memory.
//
//
//VQGen (or any other program using this dll) with VqDll.Dll
//--------------------
//
//The .dll should go in the same directory as the .exe, or in the windows directory.
//The .lib file should be included in the project that calls the Dll when the application
//is built.
*/

#ifndef MyDllExport
#define MyDllExport
#endif

/*
// Define the interface types etc.
//
// Note for IMG TECH: these MUST be the same as in vqcalc.h
*/
#ifndef VQ_OUTOFMEMORY


/* Error Return messages */

#define VQ_OUTOFMEMORY	-1
#define VQ_INVALID_SIZE -2
#define VQ_INVALID_PARAMETER -3

/*
// Settings for output format
//
// Transparent formats.
*/
#define	FORMAT_4444 (0)   /* The BEST mode for quality transparency*/
#define	FORMAT_1555 (1)	  /*This also doubles for the opaque 555 format*/

/*
// Opaque formats
*/
#define	FORMAT_555  (FORMAT_1555)
#define FORMAT_565	(2)   /*Either this of YUV is the best format for Opaque*/

#define FORMAT_YUV	(3)

/*
// for future expansion
*/
//#define FORMAT_BUMP	(4)


typedef enum
{
 	VQNoDither = 0,
	VQSubtleDither,
	VQFullDither
} VQ_DITHER_TYPES;


/*
// Colour space metrics
*/
typedef enum
{
 	VQMetricEqual = 0,	/*equal spacing*/
	VQMetricWeighted,

	/*
	// The following name is for backward compatibility only.
	*/
	VQMetricRGB = VQMetricEqual

} VQ_COLOUR_METRIC;


/*
// The following flag can be ORed with the previous values
// to form a variation on the colour metric. It makes the
// compressor tolerate greater inaccuracy in high frequency
// changes.
*/
#define VQMETRIC_FREQUENCY_FLAG 0x10
#define VQ_BASE_METRIC_MASK     0x0F


#endif







/******************************************************************************/
/*
// Function: 	VqCalc2
//
// Description: DLL version of CreateVq function.
//				Given raw, packed 24 bit RBG data (with optional separate alpha),
//				this routine generates the VQ format as used by PowerVR2.
//
// Inputs:		InputArrayRGB 	Input data as a packed array of bytes
//				InputArrayAlpha	Optional alpha values
//				OutputMemory	Supplied memory for storage of resulting VQ data.
// 								If this is NULL the function ONLY returns the size
//								of output data (in bytes) required for the given
//								parameters.
//
//				bBGROrder		if data is ordered as BGR not RGB
//				nWiddth			Texture width: Only square, power of 2 sizes currently
//								supported, and these are limited to 8x8 through to
//								1024x1024
//				ReservedA		Supply as 0
//
//				bMipMap			Generate MIP map. Recommended for rendering speed &
//								quality
//				bAlphaOn		Is alpha data supplied? If not, assumed Alpha == 0xFF
//
//				bIncludeHeader	Optionally prepends the 12byte .VQF file format data. If
//								the data is to go straight into memory OR be massaged
//								into the PVR file format, you probably don't want the
//								header data.
//
//				DitherLevel		Controls error diffusion. Ranges from none, 1/2, and full
//								(see defines above)
//
//				NumCodes		The maximum number of Vector codes to allocate
//								Note that the routine will round this value up to a
//								supported power of 2.
//
//				nColourFormat	See Codes above: One of 565, YUV, 4444, or  (1)555
//								565 or YUV probably best for Opaque texures, and
//								4444 best for translucency.
//
//				bInvertAlpha	Usually set to 0. Flips the supplied Alpha values for
//									old PVR1 input data
//
//				Metric			How to "measure" the distance between colours.
//								"VQMetricEqual" gives weighting to all RGB&A or YUV
//								components.
//								"VQMetricWeighted" attempts to tune the result a bit
//								closer to the eye's behaviour.
//
//								If the VQMETRIC_FREQUENCY_FLAG is also set on this value
//								the function will tolerate greater error in high
//								frequency areas. This sometimes helps because the eye
//								doesn't tend to notice small errors in high frequency
//								data.
//
//				ReservedB		Set it to 0.
//
// Outputs:
//				fErrorFound		returned RMS error per colour channel
//
// Returned Val:
//				IF there is an error, it returns one of the negative "Return messages"
//				listed above.
//
//				IF 	(OutputMemory == NULL), it returns the required size for the
//				output memory
//
//				IF (OutputMemort != NULL), and the function is successful, it returns
//				the number of codes actually used.
*/
/******************************************************************************/

MyDllExport int VqCalc2(void*	InputArrayRGB,
						void*	InputArrayAlpha,
						void*	OutputMemory,

						int		bBGROrder,
						int		nWidth,
						int		Reserved0,

						int		bMipMap,
						int		bAlphaOn,
						int		bIncludeHeader,

			VQ_DITHER_TYPES		DitherLevel,

						int		nNumCodes,
						int		nColourFormat,
						int		bInvertAlpha,

						VQ_COLOUR_METRIC 	Metric, /*a VQ_COLOUR_METRIC value optionally ored with	frequency flag*/
						int 	Reserved1,
						float	*fErrorFound);




/******************************************************************************/
/*
// Function: 	VqGetVersionInfoString
//
// Description: Simply returns the version of the DLL in a user supplied string.
//				Added at the request of Sega Europe.
//
//	Example:
//			VqGetVersionInfoString( szBuffer, "FileVersion" );
*/
/******************************************************************************/

MyDllExport void VqGetVersionInfoString( char* pszVersionInfoString, const char* pszKey );





/*
// The OBSOLETE function interface. ***ONLY*** for backward compatibility
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
								int		WeightMode,
								int		DummyB,
								float	DummyC[3],
								float	DummyD[4],
								int		DummyE,
								int		bInvertAlpha,
								float	DummyF,
								float	*fErrorFound);





/*
// END OF FILE
*/
