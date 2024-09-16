/******************************************************************************
 * Name			: VQCALC.C
 * Title		: Vector Quantisation Compression Code
 * Authors		: Simon Fenney, Julian Hodgson, and  Rowland Fraser
 *				  
 *
 * Modified     : 22/11/1999 by Simon Fenney
 *						Virtually a complete rewrite to allow a completely 
 *						new compression method.
 *
 * 						Redundant parameters have been removed.
 *						Currently this code uses doubles, and so probably
 *						won't be compatible with the SH4's standard FP library.
 *
 *				: 14/7/1999 by Carlos Sarria.
 *						Made changes to compile on SH4
 *
 * Created		: 17/03/1997 
 *
 * Copyright	: 1999 by Imagination Technologies Ltd. All rights reserved.
 *				: No part of this software, either material or conceptual 
 *				: may be copied or distributed, transmitted, transcribed,
 *				: stored in a retrieval system or translated into any 
 *				: human or computer language in any form by any means,
 *				: electronic, mechanical, manual or other-wise, or 
 *				: disclosed to third parties without the express written
 *				: permission of Imagination Technologies Ltd., Unit 8, HomePark
 *				: Industrial Estate, King's Langley, Hertfordshire,
 *				: WD4 8LZ, U.K.
 *
 * Description	: Code for Vector Quantisation Compression of a 2D image.
 *				  
 *
 * Platform		: ANSI (HS4/CLX2 Set 2.24)
 * 
 * Note         : CreateVq() returns a VQ format, which OPTIONALLY consists of a twelve 
 *                byte header followed by the code book data and finally the 
 *                image data without any padding between the sections.
 *                Only the code book data and the image data have to be uploaded 
 *                in video memory. If you want to use this tool for creating a 
 *                'pure' VQ format, EITHER use the appropriate parameter, or just 
 *				  remove the 12-byte header and re-align to a 32-byte boundary.
 *
 *				Note from PREVIOUS version:
 *                This code uses malloc() for allocating memory. It works fine under 
 *                VisualC++ and any other PC compilers, but we have found that its use 
 *                under SH4 compilers is not stable enough. It works as expected in 
 *                most cases, but sometimes the final texture is not aligned correctly
 *                for the hardware (it only happens when creating and loading textures 
 *                on the fly).
 *
 *
 *	RCS History:	$Log: vqcalc.c,v $
 * Revision 1.3  2000/01/28  09:34:06  sjf
 * Only a cosmetic change to simply note that this version was
 * part of the 2.01.00.0007  vqdll package.
 *
 * Revision 1.2  2000/01/28  09:29:30  sjf
 * Added rcs logging messages.
 *
 ******************************************************************************/


/******************************************************************************/
/*  INCLUDES                                                                  */
/******************************************************************************/
#include <stdlib.h>

#include <math.h>
#include <limits.h>
#include <float.h>
#include <assert.h>

#include "vqcalc.h"


/******************************************************************************/
/*
// Compile/Build options
*/
/******************************************************************************/

/*
// We need numbers with at least 2+10+10+8+8+something bits of precision
// We can use either doubles or 64 bit ints
//
// Hmm the "integer" version appears to be slower on the x86.
// A bit pointless really.
*/
#define USE_DOUBLES_FOR_MATHS (1)


#if USE_DOUBLES_FOR_MATHS

	typedef double DMTYPE;

	 /*
	 // for when we want a compatible type with less precision.
	 // Used for speed reasons (?)
	 */
	typedef float  SMTYPE;
	
#else
	#if WIN32
		typedef __int64 DMTYPE;
	#else
		typedef long long DMTYPE;
	#endif
	typedef int  SMTYPE;
#endif



/*
// decide how we store the "perception space" data. 
*/
#if WIN32 && USE_DOUBLES_FOR_MATHS

	#define PV_TYPE float
	#define PVT_IS_FLOAT (1)
#else
	#define PV_TYPE U8
	#define PVT_IS_FLOAT (0)
#endif



#define FP_COMPARE_SLOW	   (1) /*Intel again. FP compares are abysmal*/

#if FP_COMPARE_SLOW
	/*
	// a nasty macro to pretend a float is an int
	*/
	#define FLOAT_AS_INT(int_in_mem) (*((int*)(&(int_in_mem))))

#endif

/*
// The following was an experiment to add GLA passes. To be honest, it didn't
// appear to improve anything anyway, so  the "extra" GLA iterations are currently 
// turned off.
*/

#define MAX_EXTRA_GLA_ITERATIONS (6)
#define EXTRA_GLA_ITS (0)



/*
// Experimental modes for perception space weighting:
//
//  Red Blue biasing was an experiment to try a weigthing based on 
//  http://www.compuphase.com/cmetric.htm
//
// After testing with a few images and conducting a small survey at
// Img. Tech, I came to the conclusion that it's slightly
// worse than simple 3R/4, G, B/2  weighting. Of course, it may be
// another factor that is influencing the choice, but for the moment
// it should be left disabled.
*/
#define TRY_RED_BLUE_BIASING (0)

/******************************************************************************/
/*  DEBUG/BUILD options                                                       */
/******************************************************************************/

#if (DEBUG || _DEBUG )  /*just in case it's the microsoft compiler*/
	#define DEBUG 1
#else
	#define DEBUG 0
#endif


#if !DEBUG
	#define DNDEBUG (1)		 /*this should remove asserts*/
#endif



/*
// Debugging hacks
*/
#define HACK_SHOW_LOWER_MAPS     (0) /*Put the lower MIP levels in the place of the top map*/
#define HACK_FORCE_PV_TO_U8_ACC	 (0) /*Forces Perception space to only be accuracy of U8*/


#if DEBUG
	#define DEBUG_FILE 1
#else
	#define DEBUG_FILE 0  /*this gives the option of outputting debug in an optimised build*/
#endif

/*
// DEBUG File logging
*/
#if DEBUG_FILE
	/*
	// We only need IO and char strings if we are in a debug build
	*/
	#include <stdio.h>
	#include <string.h> 

	static FILE *DebFile;

	#define DEB_OUT  fprintf(DebFile, 
	
#else
	#define DEB_OUT /##/
#endif



/******************************************************************************/
/*
//  DEFINES AND TYPEDEFS
*/
/******************************************************************************/


#define LITTLE_ENDIAN (1)
typedef unsigned char U8;

/*
// the following is an internal-only flag used for YUV cases. It allows
// the use of the frequency weighting metric on JUST the Y component
*/
#define VQMETRIC_FREQUENCY_FLAG_JUST_FIRST (0x100)

/*
// also for YUV, this is the equivalent of the colour space weighting
*/
#define VQMetricWeightedYUV (15)

/**************************************
// "DEBUG" Build options
**************************************/

/*
//double check the fast lookup code against a brute force search
*/
#define CHECK_FAST_LOOKUP (0)

/*
// gather statistics on how well the fast-lookup code is doing
*/
#define GATHER_STATS (0)


#if DEBUG
	static void myassert(int expression, char * File, int line);
	#define ASSERT(X) {myAssert(X, __FILE__, __LINE__);}
#else
	#define ASSERT(X)
#endif

#if DEBUG
	int FlatCount = 0;
#endif		




/**************************************
// Useful macros
**************************************/
#define NEW(X) 	 ((X *) malloc (sizeof(X)))
#define SQ(X) ((X)*(X))
/*
// Clamp X to lie within A and B 
*/
#define CLAMP(X, A, B) if((X)>(B))(X)=(B);else if((X)<(A))(X)=(A);

#define MINMAX(TheMin, TheMax, X) if((X)>(TheMax))(TheMax)=(X);else if((X)<(TheMin)) (TheMin)=(X);


/*
// Expressions giving 3/4s, 3/8ths etc
//
// Shifts only work as divisions with positive numbers,
// hence all the messing about checking for negatives.
*/
#define THREE_4RS(X)  ((X)>=0)?(((X)*3)>>2):-((-(X)*3)>>2)
#define THREE_8THS(X) ((X)>=0)?(((X)*3)>>3):-((-(X)*3)>>3)

#define HALF(X)  ((X)>=0)?((X)>>1):(-(-(X)>>1))

/*********************************************************/
/*********************************************************/


/**************************************
// Sizes
**************************************/

/*
// Define our "max image" dimensions. We could increase these if necessary,
// but CLX (i.e. Dreamcast's PVR chip) only goes to 1kx1k textures.
//
// Ideally these values should be completely dynamic, but it makes the code
// a lot harder to write and, not only that, some of the internal values (eg Sums
// used in computing covariance matrices) can't deal with unbounded sizes anyway.
*/
#define MAX_X_PIXELS (1024)
#define MAX_Y_PIXELS (1024)

#define MAX_MIP_LEVELS (11)  /*defined by the max res: Logb2(maxres) + 1 */

/*
//Max number of Quantized Vectors
*/
#define MAX_CODES	 (256)

/*
// From now on, hardwire the dimensions of the vectors. For an
// RGB+A 2x2 this gives a vector length of 16.
//
// Note that some of the code makes assumptions about these sizes, but
// hopefully, if it's *ever* necessary to change these, I've put in 
// enough asserts to catch any problems.
*/
#define PIXEL_BLOCK_SIZE (2)
#define MAX_COMPS_PER_PIXEL (4)
#define VECLEN (MAX_COMPS_PER_PIXEL * PIXEL_BLOCK_SIZE * PIXEL_BLOCK_SIZE)


/*
// Define the weights used for lower level MIP maps so that they have
// a better chance of being mapped accurately. Ideally the weights should
// go up by a factor of four, I'd guess, to maintain accuracy of the lower
// maps, but this sacrifices the top level map too much. I've chosen to compromise
// and only double the weight each time.
//
// NOTE keep the values for TOP level maps small or we might overflow some weight calcs
*/
#if 0
static const int Weights[MAX_MIP_LEVELS] = 
{
	1,
	2,
	4,
	8, 	
	16, 	
    32, 	
    64, 	
   128, 	
   256, 	
   512, 	
  1024
};
#else
static const int Weights[MAX_MIP_LEVELS] = 
{
	1,
	1,
	2,
	4,
	8, 	
	16, 	
    32, 	
    64, 	
   128, 	
   256, 	
   512
};
#endif

/******************************************************************************
 * LOCAL Data Structures Definitions
 ******************************************************************************/

/*
// I think the quantizer MAY be memory bandwidth limited in at least on full size 
// (1kx1k) images. The structure has thus been played around with a little to try
// to get the size down
//
// The current size of the structure (with float pv) is 4 + 16*4 + 16 = 84 bytes.
// This doesn't align terribly well with the cache lines, but doesn't appear to
// make any difference anyway. 
*/
typedef struct
{
	/*
	// The true unadulterated source data (RGBA, YUV, or whatever).
	*/
 	U8  v[VECLEN];

	/*
	// pv are the colours in 'perception' space. We can either store
	// these as bytes (i.e. save lots of memory, but is VERY SLOW since
	// converting Bytes to floats on an 0x86 is hideous) or directly
	// as floats.
	*/
	PV_TYPE pv[VECLEN]; 

	/*
	// Lower level mip maps will have greater importance. The "weight"
	// makes it appear like there are "Weight" times as many copies
	// of the pixel.
	//
	// Code is the index of the representative assigned to the vector.
	//
	// We don't need weight and code simultaneously, so they're packed together.
	*/
	union
	{
		int 			Weight; 
		int 			Code;
	}wc;	

}PIXEL_VECT;



/* 
// Define a Vector Format for the image. To save a small amount of space,
// when doing images less than maximum size, but still have "easy to write"
// code, this structure has a fixed MAX number of rows, but each row is 
// dynamically allocated.
*/
typedef struct 
{
	int xVDim, yVDim;
	PIXEL_VECT *Rows[MAX_Y_PIXELS/PIXEL_BLOCK_SIZE];

}IMAGE_VECTOR_STRUCT;


/*
// Structure used to make a fast initial guess as to the 
// closest representative vector for a particular
// image vector. This is a sort of BSP tree...
*/

typedef struct _SearchTreeNode
{
	int LeafRepIndex;

	int SplittingAxis[VECLEN];
	int d;	
	
	struct _SearchTreeNode *pLess, *pMore;
}SearchTreeNode;



/*
// Structures needed for a final GLA Pass
*/
typedef struct
{
	int Sum[VECLEN];
	int Usage;
}SUM_USAGE_STRUCT;



/******************************************************************************/
/******************************************************************************/	
/*
//  Major Prototypes
*/
/******************************************************************************/
/******************************************************************************/

/*
// Vector Quantizer routine
//
//
// To do: DESCRIBE INPUTS/OUTPUTS etc
//
*/
static int VectorQuantizer(IMAGE_VECTOR_STRUCT *Maps[MAX_MIP_LEVELS],
					int NumMaps,
					int NumRepsRequired,
					int Format,
					int	bAlphaOn,
					int Metric,			/*how to estimate colour differences*/

	   SearchTreeNode 	**ppSearchRoot,

			PIXEL_VECT 	*pReps, 
					int *pVectorCount);


/*
// Map Image to vectors.
//
// Steps through the image vectors and assigns the closest representative
// to each vector.
*/
static float MapImageToIndices(IMAGE_VECTOR_STRUCT *Maps[MAX_MIP_LEVELS],
										int NumMaps,
						SearchTreeNode 	  	*pSearchRoot,
					   	const PIXEL_VECT 	*pRepVectors,
					   		          int 	NumReps,
					   	 SUM_USAGE_STRUCT 	SumAndUsage[MAX_CODES], /*debug data*/
									  int 	DiffusionLevel,
									  int	DitherJust1stComponent);


/******************************************************************************/
/******************************************************************************/

#if DEBUG
static void myAssert(int expression, char * File, int line)
{
	if(!expression)
	{
		volatile int a;
		fprintf(stderr, "Failed Assertion: File %s, Line %d\n", File, line);

		a = 1;  /* put a break point here when debugging*/
		assert(0);
	}

}
#endif


/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/*
//  Quantization Code:
*/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/*
// Partition Structure:
// This indicates a subset of the image vectors. Each may get 
// chopped into 2 further partitions depending on which ones
// have the highest error.
*/
typedef struct
{
	int Start, Length;	/*Start position and length of the partition*/
	float Error;		/*"Vector matching" error associated with the partition*/

	/*
	// Auxiliary structure used later when finding nearest vectors
	*/
	SearchTreeNode *pThisNode; 

}PARTITION_ENTRY;


/*
// When partitioning the data, we don't want to shuffle the original data
// so instead we will move pointers to the data. At the same time, we
// also store the value used to order the vectors along a particular axis.
*/
typedef struct
{
	PIXEL_VECT * pVec;
	float d;

}VECTOR_REF_STRUCT;


/****************************/


/*
// Data structure and function used to map 8bit values to lower
// resolution equivalents
*/
static const int BitDepths[4][4]=
{
 	/* FORMAT_4444*/
	4,4,4,4,   /*Red, Green, Blue, Alpha*/

	/* FORMAT_1555   -- is also 555*/ 
	5, 5, 5, 1,

	/* FORMAT_565 */
	5, 6, 5, 0,

	/* FORMAT_YUV */
	8, 8, 0, 0

};


/******************************************************************************/
/*
// ConvertBitDepth
// Given an 8 bit value and the final bit depth, this finds the best matching
// value at the lower bit depth and then expands it back to an equivalent
// full 8 bit value (as done in the PowerVR hardware).
*/
/******************************************************************************/

static U8 ConvertBitDepth(int BitDepth, int Val)
{
	/*
	// special case each one. there are only handfull
	*/
	switch(BitDepth)
	{
		case 4:
		{
			/*
			// Convert down to 4 bit
			*/
			Val = (int) (Val * (15.0f/ 255.0f) + 0.5f);

			/*
			// Convert back to equivalent 8 bit
			*/
			Val = Val | (Val << 4);
			break;
		}

		case 5:
		{
			/*
			// Convert down to 5 bit
			*/
			Val = (int) (Val * (31.0f / 255.0f) + 0.5f);

			/*
			// Convert back to equivalent 8 bit
			*/
			Val = (Val >> 2) | (Val << 3);
			break;
		}

		case 6:
		{
			/*
			// Convert down to 6 bit
			*/
			Val = (int) (Val * (63.0f / 255.0f) + 0.5f);

			/*
			// Convert back to equivalent 8 bit
			*/
			Val = (Val >> 4) | (Val << 2);
			break;
		}

		case 8:
		case 0: /*the zero case here allows for "null" values in the table of bit depths*/
		{
			/*
			// Don't change it
			*/
			break;
		}

		/*
		// Default: Assume it's one!
		*/
		default:
		{
			ASSERT(BitDepth == 1);

			if(Val > 127)
			{
				Val = 255;
			}
			else
			{
				Val = 0;
			}
		}

	}/*end switch*/

	return (U8) Val;
}





/*********************************************************/
/*
// SumToRep
//
// Converts the sum of vector values and the number in that sum, to
// the reduced colour resolution version. It uses a little bit
// of error diffusion in the process.
//
// The error diffusion process COULD be made better I suspect.
//
// Note: We have to special case the YUV conversion because both U and
// V live in the same "offset" and we really don't want to copy the
// values over.	Furthermore, since the Y values are 8 bit anyway, we
// don't both trying to do diffusion on the "fractional" bit of 
// accuracy lost when converting from float to int.
//
*/
/*********************************************************/
static void SumToRep(int Sum[VECLEN], int Number, int Format, 
					 PIXEL_VECT *pRep)
{
	float InvNum;
	int k;

	int Errors[MAX_COMPS_PER_PIXEL];

	InvNum = 1.0f / (float)Number;


	
	for(k = 0; k < MAX_COMPS_PER_PIXEL; k++)
	{
		Errors[k] = 0;
	}

	for(k = 0; k < VECLEN; k++)
	{
		int Val;
		U8 	ValOut;

		int ThisComponent;

		/*
		// get the average with rounding
		*/
		Val = (int) (Sum[k] * InvNum + 0.5f);
		
		/*
		// YUV is 8 bits anyway: don't worry about diffusion.
		*/
		if(Format == FORMAT_YUV)
		{
			ValOut = (U8) Val;
		}
		else
		{
			/*
			// Convert Val to the reduced number of bits, and then
			// back to the equivalent with 8 bits
			//
			// Because we can lose so much precision in converting to the
			// lower bit depth values, do some error diffusion within the 
			// representative vector. 
			//
			// NOTE, rather than do a % op, we use &, since we know	 MAX_COMPS_PER_PIXEL=4
			*/
			ASSERT(MAX_COMPS_PER_PIXEL == 4); 
			ThisComponent = k & (MAX_COMPS_PER_PIXEL-1);

		
			/*
			// add in the error carried over from the previous pixel, and clamp it
			*/
			Val = Val + Errors[ThisComponent];
			CLAMP(Val, 0, 255);

			/*
			// Convert to the lower rep
			*/
			ValOut = ConvertBitDepth(BitDepths[Format][ThisComponent], Val);

			/*
			// get the error associated with this reduction
			*/
			Errors[ThisComponent] = Val - ValOut;
		}/*end if format*/

		pRep->v[k] = ValOut;
 	}/*end for k*/

}


/*********************************************************/
/*
// Squash
//
// Squeezes up numbers so that the curve is still monotonic, but
// the deltas get smaller as you get further away from zero....
// PROVIDED 0 < PowCompression < 1.
*/
/*********************************************************/

static float Squash(float Val, float PowCompression)
{
	int Neg;
	
	Neg = (Val < 0.0f);
	Val = (float) fabs(Val);

	Val = (float) pow(Val, PowCompression);
	
	if(Neg)
	{
		Val = -Val;
	}
	
	return Val;
}


/*********************************************************/
/*
// RawToPerceptionSpace
//
// Converts the raw values into the colour space used by the 
// quantizer. 
*/
/*********************************************************/
 
static void RawToPerceptionSpace(const int 	Metric,
								  const U8 	VecIn[VECLEN],
								PV_TYPE 	VecOut[VECLEN])
{
	int j;

	/*
	// If keep all components as is, just copy over the data
	*/
	if((Metric & VQ_BASE_METRIC_MASK) == VQMetricEqual)
	{
		for(j = 0; j < 4; j++)
		{
			int Src;

			Src = j * 4;

			VecOut[j]    = VecIn[Src];
			VecOut[j+4]  = VecIn[Src+1];
			VecOut[j+8]  = VecIn[Src+2];
			VecOut[j+12] = VecIn[Src+3];
		}


	}
	/*
	// Else if use ARGB weighted values.
	*/
	else if((Metric & VQ_BASE_METRIC_MASK) == VQMetricWeighted)
	{
		for(j = 0; j < 4; j++)
		{
			float AlphaWeight;
			float RModWeight;
			int Src;

			Src = j * 4;
				 
			AlphaWeight = 0.25f + (VecIn[Src+3] * (3.0f/(4*255.0f)));

#if TRY_RED_BLUE_BIASING
			RModWeight = VecIn[Src] * (1.0f/ (4.0f * 255.0f));
#else
			RModWeight = 0.25f;
#endif

			/*
			// Weight R,G & B
			*/
#if HACK_FORCE_PV_TO_U8_ACC
			/*
			// In this debug mode, even if we are using floats for pv, these'll still be clamped
			// to 8bit values. This makes debugging easier because the last Error
			// values in "FindPartition" should return to zero.
			*/
			VecOut[j]   =  (U8) (AlphaWeight * (0.5f  + RModWeight) * VecIn[Src]);
			VecOut[j+4] =  (U8) (AlphaWeight * 					      VecIn[Src+1]);
			VecOut[j+8] =  (U8) (AlphaWeight * (0.75f - RModWeight) * VecIn[Src+2]);
#else
			VecOut[j]   =  (PV_TYPE) (AlphaWeight * (0.5f  + RModWeight) * VecIn[Src]);
			VecOut[j+4] =  (PV_TYPE) (AlphaWeight * 					   VecIn[Src+1]);
			VecOut[j+8] =  (PV_TYPE) (AlphaWeight * (0.75f - RModWeight) * VecIn[Src+2]);
#endif
			/*
			// Copy alpha as is
			*/
			VecOut[j+12] = VecIn[Src+3];
		}
	}
	/*
	// Else use YUV weights. We simply leave the Y as is, and squash down the U/V
	*/
	else
	{
		for(j = 0; j < 4; j++)
		{
			int Src;

			Src = j * 4;
				 
			/*
			// Weight Y and U/V
			*/
#if HACK_FORCE_PV_TO_U8_ACC
			/*
			// In this debug mode, even if we are using floats for pv, these'll still be clamped
			// to 8bit values. This makes debugging easier because the last Error
			// values in "FindPartition" should return to zero.
			*/
			VecOut[j]   =  (U8) VecIn[Src];
			VecOut[j+4] =  (U8) (0.75f * VecIn[Src+1]);
#else
			VecOut[j]   =  (PV_TYPE) VecIn[Src];            
			VecOut[j+4] =  (PV_TYPE) (0.75f * VecIn[Src+1]);
#endif
			/*
			// Set the two redundant values to zero
			*/
			VecOut[j+8]  = (PV_TYPE) 0;
			VecOut[j+12] = (PV_TYPE) 0;
		}
	}/*end if basic colour weight choice*/


	/*
	// determine if we need to convert to frequency space
	*/
	if (Metric & (VQMETRIC_FREQUENCY_FLAG | VQMETRIC_FREQUENCY_FLAG_JUST_FIRST))
	{
		int Len;

		/*
		// decide if we do all the components or just the first
		*/
		if(Metric & VQMETRIC_FREQUENCY_FLAG)
		{
		 	Len = VECLEN;
		}
		else
		{
			Len = 1;
		}
		/*
		// Convert this into "Frequency Space" using a Hadamard transform...
		*/
		for(j = 0; j < Len; j+=4)
		{			
			float A00, A01, A10, A11;
			float B00, B01, B10, B11;

			A00 = VecOut[j];
			A01 = VecOut[j+1];
			A10 = VecOut[j+2];
			A11 = VecOut[j+3];

			/*
			// Do Hadamard transform
			*/
			B00 = (A00 + A01 + A10 + A11) * 0.25f;
			B01 = (A00 - A01 + A10 - A11) * 0.25f;
			B10 = (A00 + A01 - A10 - A11) * 0.25f;
			B11 = (A00 - A01 - A10 + A11) * 0.25f;

			/*
			// write these back, doing some pseudo log/power compression
			*/
#if HACK_FORCE_PV_TO_U8_ACC
			VecOut[j]   = (U8) (B00 + 0.5f);
			VecOut[j+1] = (U8) (Squash(B01, 6.0f / 8.0f) + 64.0f);
			VecOut[j+2] = (U8) (Squash(B10, 6.0f / 8.0f) + 64.0f);
			VecOut[j+3] = (U8) (Squash(B11, 6.0f / 8.0f) + 64.0f);
#else
			VecOut[j]   = (PV_TYPE) (B00 + 0.5f);
			VecOut[j+1] = (PV_TYPE) (Squash(B01, 6.0f / 8.0f) + 64.0f);
			VecOut[j+2] = (PV_TYPE) (Squash(B10, 6.0f / 8.0f) + 64.0f);
			VecOut[j+3] = (PV_TYPE) (Squash(B11, 6.0f / 8.0f) + 64.0f);
#endif

		}/*end for j*/
	}/*end if convert to frequency space*/
}


/*********************************************************/
/*
// 	Jacobi:
//
// This is based upon Julian's modified version of the code in 
//   Numerical Recipes in C  page 463-469
//
// Note I have further modified the code so that indices are relative to ZERO and
// not 1.
//
//
// Computes all eigenvalues and eigenvectors of the real symmetric matrix,
//   		a[VECLEN][VECLEN].
// On output, elements of "a" above the diagonal are destroyed. 
// d[VECLEN] returns the eigenvalues of "a".
// v[VECLEN][VECLEN] is a matrix whose *columns* contain, on output, the normalized 
// eigenvectors of a. 
//
// n specifies the dimension so we can do 12x12, 16x16 or whatever.
//
// The function returns the number of Jacobi rotations that were required - this
// is more for interest sake :-)
*/
/*********************************************************/

#define ROTATE(a,i,j,k,l) g=a[i][j];h=a[k][l];a[i][j]=g-s*(h+g*tau);\
	a[k][l]=h+s*(g-h*tau);



static int jacobi(float a[VECLEN][VECLEN], 
			  const int n, 
				  float d[VECLEN], 
				  float v[VECLEN][VECLEN])
{
	int nrot;/*number of rotations performed*/

	int j,iq,ip,i;
	float tresh,theta,tau,t,sm,s,h,g,c;
	
	float b[VECLEN],z[VECLEN];


	/*
	// Initialise V to be the identity matrix
	*/	
	for (ip=0; ip < n; ip++)
	{
		for (iq=0; iq < n; iq++)
		{
			v[ip][iq]= 0.0f;
		}
		v[ip][ip]=1.0f;
	}

	/*
	// set B and D to be the diagonal of "a"
	*/
	for (ip=0; ip < n; ip++)
	{
		b[ip]=d[ip]=a[ip][ip];
		z[ip]=0.0f;
	}

	/*
	// initialise the count of the number of jacobian rotations
	*/
	nrot=0;

	/*
	// perform some number of passes until we get convergence, i.e. a
	// diagonalised matrix
	*/
	for (i=1; i<=50; i++)
	{
		/*
		// Sum the off-diagonal elements - this gives us a measure of how
		// "undiagonal" our matrix is.
		*/
		sm=0.0f;
		for (ip = 0;ip < n-1; ip++)
		{
			for (iq=ip+1; iq < n; iq++)
			{
				sm += ( float) fabs(a[ip][iq]);
			}
		}

		/*
		// If the matrix has become diagonalised, then we're done. This relies
		// on the quadratic convergence of the method along with machine underflow
		// clamping things to zero.
		*/
		if (sm == 0.0) 
		{
			return nrot;
		}

		/*
		// Special case the first few rotations: use a larger threshold to start
		// with. See page 466 of "Numerical Recipes in C"
		*/
		if (i < 4)
		{
			tresh=0.2f*sm/(n*n);   /*the first 3 sweeps*/
		}
		else
		{
			tresh=0.0f;
		}

		/*
		// iterate through the upper triangle of the matrix
		*/
		for (ip=0;ip < n-1; ip++)
		{
			for (iq=ip+1; iq < n; iq++) 
			{
				g= 100.0f * (float) fabs(a[ip][iq]);

				/*
				// If after 4 sweeps, skip the rotation if the off-diagonal is small
				*/
				if ((i > 4) && 
					(float)(fabs(d[ip])+g) == (float)fabs(d[ip]) && 
					(float)(fabs(d[iq])+g) == (float)fabs(d[iq]))
				{
					a[ip][iq]=0.0f;
				}
				else if (fabs(a[ip][iq]) > tresh)
				{
					h=d[iq]-d[ip];
					if ((float)(fabs(h)+g) == (float)fabs(h))
					{
						t=(a[ip][iq])/h;
					}
					else
					{
						theta=0.5f*h/(a[ip][iq]);
						t=1.0f/(( float) fabs(theta)+( float) sqrt(1.0+theta*theta));
						if (theta < 0.0) t = -t;
					}
					c=1.0f/ (float) sqrt(1+t*t);
					s=t*c;
					tau=s/(1.0f+c);
					h=t*a[ip][iq];
					z[ip] -= h;
					z[iq] += h;
					d[ip] -= h;
					d[iq] += h;
					a[ip][iq]=0.0f;

					/*
					// do rotations 0 <= j < p
					*/
					for (j=0; j < ip; j++) 
					{
						ROTATE(a,j,ip,j,iq)
					}

					/*
					// do rotations p < j < q
					*/
					for (j=ip+1; j < iq; j++)
					{
						ROTATE(a,ip,j,j,iq)
					}

					/*
					// do rotations  q < j < n
					*/
					for (j=iq+1; j < n; j++) 
					{
						ROTATE(a,ip,j,iq,j)
					}

					for (j = 0;j < n;j++) 
					{
						ROTATE(v,j,ip,j,iq)
					}
					nrot++;
				}/*end if value is significant*/
			}
		}/*end for ip*/

		for (ip=0; ip < n; ip++) 
		{
			b[ip] += z[ip];
			d[ip]=b[ip];
			z[ip]=0.0f;
		}
	}/*end for do iterations until we diagonalise the matrix*/

	DEB_OUT "Too many iterations in routine jacobi");

	return nrot++;
}


/*********************************************************/
/*
// 	GenerateAxis:
//
// This function generates the Covariance matrix of the set of
// vectors in the given partition. From this it finds the best
// splitting axis, which is subsequently used to further divide
// this partition.
//
//
// NOTE: From profiling, this routine can take up to 50% of the time
//		of the whole VQ process. 
//		The process of computing the Covariance matrix is proportional
//		to the square of dimension of the vectors. We can thus get
//		a potential increase in speed (nearly 50% comparing 16x16 V. 12x12)
//		by having a special case for when we aren't really processing
//		alpha. (Actually on a PC this didn't appear to materialise, probably
//		because it's limited by the memory subsystem)
//
//
//	NOTE2: This routine assumes NumDims is divisible by 4
//
*/
/*********************************************************/
static void GenerateAxis(const VECTOR_REF_STRUCT * pPartitionStart, 
						    int NumVectors,
						 SMTYPE MainAxis[VECLEN],
					  const int	NumDims,
						 DMTYPE	OutSQSums[VECLEN],
						 DMTYPE	OutSums[VECLEN],
						 	int	*pOutWeightSum)
{
	/*
	// We use DMTYPEs here because we need around 36-40 bits of precision.
	// This IS going to cause a problem with the SH4. I suppose we will need
	// to re-write the code to use long long, or some other hack.
	//
	// NOTE We start with only the upper triangle of the Covariance matrix
	// because of its symmetry. In theory, we could save space, by not
	// storing the lower triangle, but it'd make the coding somewhat 
	// more difficult!
	*/
 	DMTYPE Cov[VECLEN][VECLEN];

	/*
	// a floating point version of the matrix for calculating the
	// eigenvectors and eigenvalues
	*/
	float fCov[VECLEN][VECLEN];
	float EVects[VECLEN][VECLEN];
	float EVals[VECLEN];
	

	DMTYPE Sum[VECLEN];
	int WeightSum;
	double InvWeightSum;

	int v, i, j;

	/*
	// The following are used to find the principal axis which is
	// the eigenvector of the Covariance Matrix with the largest
	// eigenvalue.
	*/
	float MaxVal;
	int MaxEigen;


	/*
	// due to optimisations in the looping code, we are assuming that
	// the number of dimensions is divisible by 4
	*/
	ASSERT((NumDims & 3)==0);

	/*
	// Initialise the Covariance matrix values. We only need to do the
	// upper triangle (includes leading diagonal)
	//
	// NOTE the unused dimensions (eg alpha if opaque)
	// will remain zero.
	*/
	for(i = 0; i < VECLEN; i++)
	{
		Sum[i] = (DMTYPE) 0;

		for(j = i; j < VECLEN; j++)
		{
			Cov[i][j] = (DMTYPE) 0;
		}
	}
	WeightSum = 0;



	/*
	// Step through all the vectors
	*/
	for(v = NumVectors; v != 0 ; v--)
	{
		const PIXEL_VECT * pVec;
		SMTYPE Elem1;
		int Weight;

		/*
		// get convenient access to the vector data
		*/
		pVec = pPartitionStart->pVec;

		/*
		// Get hold of the weight for this vector.
		*/
		Weight = pVec->wc.Weight;
		WeightSum+= Weight;
				
		/*
		// Incorporate this vector into the covariance matrix
		// calculations.
		//
		// Note, we only do the alpha components when necessary
		//
		// ***This loop is one of the most expensive parts of
		// the whole VQ computation. ***
		*/
		for(i = 0; i < NumDims; i++)
		{
			DMTYPE *CovRow;

			/*
			// Get one of the pair of values, and so save
			// multiplies later, pre-multiply it by the vector
			// weight.
			*/
			Elem1 = pVec->pv[i] * Weight;

			/*
			// Add it to the sum (for the calculating the average )
			*/
			Sum[i] += Elem1;


			/*
			// Do a small amount of loop unwinding to try to overcome
			// some of the latencies of floating point units, and to
			// get better parallelism.
			//
			// Try to do 4 calcs at a time. Begin by iterating up to
			// the first multiple of 4. NOTE: This is why we must have
			// NumDims divisible by 4.
			*/

			/*
			// get access to this row of the matrix
			*/
			CovRow = Cov[i];

			for(j = i; (j & 0x3); j++)
			{
				CovRow[j] += Elem1 * pVec->pv[j];
			}

			/*
			// Do the remainder in blocks of 4
			*/
			for(/*nil*/; j < NumDims; j+= 4)
			{
				/*
				// Use temp variable so the compiler knows there are no
				// memory aliasing problems. Whether it makes use of this
				// fact, is a completely different matter :-)
				*/
				DMTYPE Tmp0, Tmp1, Tmp2, Tmp3;

				Tmp0 = CovRow[j]   + Elem1 * pVec->pv[j];
				Tmp1 = CovRow[j+1] + Elem1 * pVec->pv[j+1];
				Tmp2 = CovRow[j+2] + Elem1 * pVec->pv[j+2];
				Tmp3 = CovRow[j+3] + Elem1 * pVec->pv[j+3];

				CovRow[j]   = Tmp0;
				CovRow[j+1] = Tmp1;
				CovRow[j+2] = Tmp2;
				CovRow[j+3] = Tmp3;
			} 
		}/*end for i*/
		pPartitionStart ++;
	}/*end for v*/

	InvWeightSum = 1.0 / (double) WeightSum;

	DEB_OUT "weightsum:%d Inverse WeightSum: %f\n", WeightSum, InvWeightSum);
	/*
	// To save recalculation work in the FindPartition Routine,
	// save the sum values we have so far computed
	*/
	for(i = 0; i < NumDims; i++)
	{
		OutSQSums[i] = Cov[i][i];
		OutSums[i]	 = Sum[i];

		DEB_OUT "%d: SQSums[]:%f Sums[]:%f\n", i, OutSQSums[i], OutSums[i]);
	}

	*pOutWeightSum = WeightSum;

	/* 
	// The following code cuts some corners:
	//
	// The real covariance values are computed from...
	//
	// Cov[i][j] = Average(Val[i] * Val[j]) - Average(Val[i]) * Average(Val[j])
	//
	// Since we are only interested in relative values, we can multiply through
	// by the weight sum (i.e as in the averaging bit)
	//
	// Again, due to symmetry, only do the upper triangle
	*/
	for(i = 0; i < NumDims; i++)
	{
		for(j = i; j < NumDims; j++)
		{
			Cov[i][j] -= (DMTYPE)(Sum[i] * Sum[j] * InvWeightSum);
		}
	}

	/*
	// Now copy this to our floating point version, and fill in the
	// entire matrix.
	*/
	for(i = 0; i < NumDims; i++)
	{
		/*
		// allow for the symmetry
		*/
		for(j = 0; j < i; j++)
		{
			fCov[i][j] = (float) Cov[j][i]; 

			DEB_OUT "%f ", fCov[i][j]);
		}
		DEB_OUT "\n");

		for(/*nil*/; j < NumDims; j++)
		{
			fCov[i][j] = (float) Cov[i][j]; 
		}
	}

	/*
	// Compute the Eigenvectors and Eigenvalues
	*/
	jacobi(fCov, NumDims, EVals, EVects);


	/*
	// Find the largest Eigenvalue. I'm not sure whether negative ones
	// are SUPPOSED to come out of the routine, but I get the occasional
	// small negative value. I'll ignore them for the present.
	*/	
	MaxVal   = 0.0f;
	MaxEigen = 0;
	for(i = 0; i < NumDims; i++)
	{
	#if 0
		if(MaxVal < (float) fabs(EVals[i]))
	#else
		if(MaxVal < EVals[i])
	#endif
		{
			MaxVal   = (float) fabs(EVals[i]);
			MaxEigen = i;
		}
	}


	/*
	// Grab the corresponding eigenvector - this is the principal
	// axis. It is in the corresponding **column** of the eigenvec matrix .
	*/
	DEB_OUT "EigenVect:%d", MaxEigen);

	for(i = 0; i < NumDims; i++)
	{
	#if	USE_DOUBLES_FOR_MATHS
		MainAxis[i] = EVects[i][MaxEigen];
	#else
		MainAxis[i] =(SMTYPE) (EVects[i][MaxEigen] * (INT_MAX >> (8+4)));
	#endif

		DEB_OUT " %f ", MainAxis[i]);
	#if DEBUG_FILE
		if(i%8==7)
		{
			DEB_OUT "\n");
		}
	#endif

	}
	DEB_OUT "\nwith EigenValue %f (hex)%x\n\n", MaxVal, FLOAT_AS_INT(MaxVal));

	
}


/*********************************************************/
/*********************************************************/


/*********************************************************/
/*
// Shell Sort
//
// I was originally using quicksort to sort the vectors along
// the axis, which was wonderful until it hit a pathological image
// (pictures of a star field. Most of the image was black). 
// Quicksort rapidly became VerySlowsort :-(
//
//
// This routine was obtained from the web, and in the process
// I came up with a new proverb...
//
// "Beware not only Greeks bearing gifts, 
//     but also Pascal programmers bearing C"
//
// The original was basing arrays at index 1, which I hadn't
// initially checked.
//
*/
/*********************************************************/


static void shellsort(VECTOR_REF_STRUCT * Partition,
					  int  NumVectors)
{
	int i, j;
	VECTOR_REF_STRUCT TempVecRef;
	int StepSize;	

	#if FP_COMPARE_SLOW
		int CompVal;
	#endif
	
	/*
	// determine the "increment" starting value for 
	*/
	for(StepSize = 1; StepSize <= NumVectors/9; StepSize = ( 3 * StepSize + 1 ) )
	{
	 	/*Nothing*/
	}/*end for h*/

	/*
	// Step through decreasing sizes of incr eg 40, 13, 4, 1 ...
	//
	// I have seen a version that halves the value each time, but
	// maybe this one might be a bit kinder on CPU caches (i.e. less
	// chance of it thrashing itself?)
	*/
	for( /*nil*/ ; StepSize > 0; StepSize /= 3 )
   	{
		/*
		// Do a simple insertion sort on this subset of values
		*/
    	for ( i = StepSize; i < NumVectors; i++ )
    	{
    		TempVecRef = Partition[i];
         	j = i;

			/*
			// two versions of the inner loop. One for decent CPUs and the
			// other for 0x86s
			*/
		#if !FP_COMPARE_SLOW
         	while ( ( j >= StepSize ) && ( Partition[j - StepSize].d > TempVecRef.d) )
         	{
            	Partition[j] = Partition[j-StepSize];
            	j -= StepSize;
         	}

		#else  
			/******
			// the horrible version for 0x86's. Pretend floats are ints
			*******/
			/*
			// turn the float value into an "integer"
			*/
			CompVal = FLOAT_AS_INT(TempVecRef.d);

			/*
			// If either of the values being compared are positive (or zero)
			// then we can use integer compares directly...
			*/
			if(CompVal >= 0)
			{
         		while ( ( j >= StepSize ) && ( FLOAT_AS_INT(Partition[j - StepSize].d) > CompVal) )
         		{
            		Partition[j] = Partition[j-StepSize];
            		j -= StepSize;
         		}
			}
			/*
			// BUT if both values are negative, then we'll get it wrong. 
			// Guarantee we'll be safe by negating both values (i.e. at least one will
			// now be positive :) ) and reversing the test.
			*/
			else
			{
				CompVal = -CompVal;

		        while ( ( j > StepSize ) && ( -FLOAT_AS_INT(Partition[j - StepSize].d) < CompVal) )
         		{
            		Partition[j] = Partition[j-StepSize];
            		j -= StepSize;
         		}
			}
		#endif

		/*
		// Note: The following is sometimes a waste, i.e. when i==j we are
		// writing back exactly the same value. I've tried testing for this redundant
		// case to see if it improved performance, but the gain seemed 
		// insignificant, so I removed it.
		//
		//if(i!=j){Partition[j] = TempVecRef;}
		*/
		Partition[j] = TempVecRef;
		
      }/*end for i*/
   }/*end for h*/	
}



/*********************************************************/
/*
// SortAlongAxis
//
// Given the vectors in the partition and the major axis, compute
// each vectors position along the axis, and then sort them by
// this value.
*/
/*********************************************************/

static void SortAlongAxis(VECTOR_REF_STRUCT * pPartitionStart, 
						    int NumVectors,
					const   int NumDims,
				    const SMTYPE Axis[VECLEN])
{

	VECTOR_REF_STRUCT * pRefVec;
	int i;
	int j;

	/*
	// compute d value
	*/
	pRefVec = pPartitionStart;


	/*
	// We assume that NumDims is divisible by 4 so that we can unwind the loop
	*/
	ASSERT((NumDims & 3)==0);


	for(i = NumVectors; i != 0; i--)
	{
		float Val;
		const PIXEL_VECT * pVec;

		pVec = pRefVec->pVec;


		/*
		// Dot the colour with the vector axis
		//
		// We use a partially unwound version of the loop
		*/
		Val = 0.0f;
		for(j = 0; j < NumDims; j+=4)
		{
			Val += (float) (Axis[j]   * pVec->pv[j] + 
							Axis[j+1] * pVec->pv[j+1] +
							Axis[j+2] * pVec->pv[j+2] +
							Axis[j+3] * pVec->pv[j+3]);
		}
		pRefVec->d = Val;
		pRefVec++;

	}/*end for i*/



	/*
	// Sort these into order. I originally used quicksort until I struck
	// a pathological image (i.e. 95% of the colours were at one extreme),
	// and the run time wasn't O(N log(N)) - more like O(N^2). This was 
	// bad considering there  were 256K vectors to sort!!
	//
	// Shell sort seems to behave more evenly..
	*/
	shellsort(pPartitionStart, NumVectors);
}


/*********************************************************/
/*
// FindPartition
//
// Given the "now sorted" vectors in the partition, try "all" the 
// different partitioning splits, and determine the one that gives
// the least overall error.
//
// NOTE: This function uses doubles because we need around 30 to 40 bits
// of precision in order to make the running time linear.
*/
/*********************************************************/

static void FindPartition(const VECTOR_REF_STRUCT * pPartitionStart, 
		PARTITION_ENTRY *pOrigPart, 
		PARTITION_ENTRY *pNewPart,
		const int 		NumDims, 
		const DMTYPE 	InSQSums[VECLEN],
		const DMTYPE 	InSums[VECLEN],
		const int	 	WeightSum)
{
	
	const VECTOR_REF_STRUCT * pTmp;

	int CurrentSplitPoint;


	/*
	// the error values have to be DMTYPEs or we lose too much precision
	// to do incremental evaluation
	*/
	DMTYPE Err1, Err2; 

	DMTYPE SumSquared1, SumSquared2;
	DMTYPE Sums1[VECLEN], Sums2[VECLEN];

	/*
	// Sum of the weights. Note we can use ints here provided the 
	// max weight used * Max_res * Max_res is less than 2^31
	*/
	int  WeightSum1,  WeightSum2;	

	/*
	// The best partition we have so far located
	*/	
	int BestSplitSoFar;
	DMTYPE BestErr1, BestErr2, BestErrSum;

	int j;


	/*
	// Start with the partition point at one extreme, with the
	// "first partition" empty, and the second partition containing
	// every data vector in the original partition.
	//
	// The first partition split is empty, so the values are zero
	*/
	WeightSum1  = 0;
	SumSquared1 = (DMTYPE) 0;
	for(j = 0; j < NumDims; j++)
	{
		Sums1[j] = (DMTYPE) 0;
	}

	/*
	// the other partition is everything else, so just copy the values for
	// the entire original partition
	*/
	WeightSum2 = WeightSum;

	SumSquared2 = (DMTYPE) 0;
	for(j = 0; j < NumDims; j++)
	{
		SumSquared2 += 	InSQSums[j];
		Sums2[j] 	= 	InSums[j];
	}


	/*
	// Initialise the best split point to be "invalid", 
	// and choose an error value we will immediately improve upon
	*/
	BestSplitSoFar = -1;

	BestErrSum = (DMTYPE) INT_MAX * INT_MAX;
	BestErr1   = (DMTYPE) 0; /*init these too, just to avoid compiler warnings*/
	BestErr2   = (DMTYPE) 0;

	/*
	// step through all possible partitions
	//
	// CurrentSplitPoint refers to the first element of the second partition
	*/
	pTmp = pPartitionStart;
	for(CurrentSplitPoint = 1; CurrentSplitPoint < pOrigPart->Length;  CurrentSplitPoint++)
	{
		const PIXEL_VECT *pThisVec;

		SMTYPE Val;
		int Weight;
		SMTYPE ColourSquared;

		/*
		// Move the next vector from the 2nd partition to the first, 
		// by updating the Sum and Sum of Sq values for each partition.
		//
		// Get easy access to the vector data
		*/
		pThisVec = pTmp->pVec;

		/*
		// grab the vector's weight
		*/
		Weight = pThisVec->wc.Weight;

		ColourSquared = (SMTYPE) 0;
		for(j = 0; j < NumDims; j++)
		{
			Val = pThisVec->pv[j];

			ColourSquared += SQ(Val);
			Val *= Weight;
			 
			Sums1[j] += Val;
			Sums2[j] -= Val; 
		}
		
		SumSquared1 += ColourSquared * Weight;
		SumSquared2 -= ColourSquared * Weight;

		WeightSum1 += Weight;
		WeightSum2 -= Weight;

	  	pTmp++;

		/*
		// Compute the errors for the two trial partitions
		*/
		Err1 = (DMTYPE) 0;
		Err2 = (DMTYPE) 0;
		for(j = 0; j < NumDims; j++)
		{
			Err1 += SQ(Sums1[j]);
			Err2 += SQ(Sums2[j]);
		}

		Err1 = SumSquared1 - (DMTYPE)(Err1 / (double)(WeightSum1));

		Err2 = SumSquared2 - (DMTYPE)(Err2 / (double)(WeightSum2));


		/*
		// If we have found a better splitting point (i.e. lower error)
		// then keep the details
		*/
		if((Err1 + Err2) < (BestErrSum))
		{
			BestErrSum = Err1 + Err2;

			BestErr1 = Err1;
			BestErr2 = Err2;
			
			BestSplitSoFar = CurrentSplitPoint;
		}
	}/*end for*/


	#if DEBUG && 0
		printf("Split %d long partition into %d and %d. Error Decrease:%f", 
				pOrigPart->Length, 
				BestSplitSoFar,
				pOrigPart->Length - BestSplitSoFar,
				pOrigPart->Error - BestErr2 - BestErr1);

	#endif
	/*
	// Return what we consider to be the best partitioning
	*/
	pNewPart->Start  = pOrigPart->Start + BestSplitSoFar;
	pNewPart->Length = pOrigPart->Length - BestSplitSoFar;
	pNewPart->Error  = (float)BestErr2;


	pOrigPart->Length = BestSplitSoFar;
	pOrigPart->Error  = (float)BestErr1;

}



/*********************************************************/
/*
// FreeSearchTree
//
// This just deallocates the "tree" of representative vectors
// that is used to speed the "nearest"
*/
/*********************************************************/
static void FreeSearchTree(SearchTreeNode * pSearchTree)
{
	if(pSearchTree != NULL)
	{
		if(pSearchTree->LeafRepIndex < 0)
		{
			FreeSearchTree(pSearchTree->pLess);
			FreeSearchTree(pSearchTree->pMore);
		}
		free(pSearchTree);
	}
}

/*********************************************************/
/*********************************************************/
/*
// Basic Vector quantization
//
// Given the set of codes, it maps these into perception space and then
// progressively partitions the original set into smaller sets. The "centre"
// of each set then becomes a representative.
//
// At each iteration the routine:
// 		*Chooses the partition (i.e. set of vectors) with the greatest error
//
//      *Finds the principal axis of the data in the set. This is done
// 		by computing the principal eigenvector of the covariance matrix
//		of the vectors in the partition.
//
//		*The vectors are then sorted along this axis by taking the dot product of
//		 each vector and the axis.
//
//		*A "locally greedy" algorithm is then used to split the set into two parts
//		 so that the sum of the errors of these two new partitions is a minimum.
//
// The function returns the number of reps computed. Occasionally this will
// be less than the number requested.
//
// If it returns VQ_OUTOFMEMORY, then there's been a memory allocation failure.
*/

static int VectorQuantizer(IMAGE_VECTOR_STRUCT *Maps[MAX_MIP_LEVELS],
				int 	NumMaps,
				int 	NumRepsRequired,
				int 	Format,
				int		bDoingAlpha, 	/*this is for optimisation purposes*/
				int 	Metric,			/*how to estimate colour differences*/

	SearchTreeNode 		**ppSearchRoot,
		PIXEL_VECT 		*pReps,
				int 	*pVectorCount)
{

	VECTOR_REF_STRUCT *pSrcVectRefs;

	int NumSrcVectors;

	/*
	// partition table
	*/
	PARTITION_ENTRY Parts[MAX_CODES];
	int NumPartitions, i, j, k;
	
	VECTOR_REF_STRUCT *pPartitionStart, *pTmpVecRef;

	/*
	// these are used to identify the next partition to subdivide
	*/
	float WorstErrorFound;
	int   WorstPartition;

	/*
	// For optimisation purposes, this stores the number of dimensions
	// we actually need. For full ARGB, we need 16, plain RGB is 12, and
	// YUV requires 8
	*/
	int   NumDims;

	/*
	// For a particular partition, this is the principal axis of the
	// the data set (ie. it's sort of the longest axis through the data)
	*/
	SMTYPE MainAxis[VECLEN];

	DMTYPE SQSums[VECLEN], Sums[VECLEN];
	int WeightSum;

	SearchTreeNode *pLess, *pMore;


	/*
	// some minor checks
	*/
	ASSERT(NumRepsRequired <= MAX_CODES);

	/*
	// compute the number of vectors
	*/
	NumSrcVectors = 0;
	for(k = 0; k < NumMaps; k++)
	{
		NumSrcVectors += (Maps[k]->xVDim * Maps[k]->yVDim);
	}
	*pVectorCount = NumSrcVectors;


	/*
	// Allocate the array that points to all the source
	// vectors
	*/
	pSrcVectRefs = malloc(sizeof(VECTOR_REF_STRUCT) * NumSrcVectors);

	/*
	// if this fails, abort out of here
	*/
	if(pSrcVectRefs == NULL)
	{
		return VQ_OUTOFMEMORY;
	}


	pTmpVecRef = pSrcVectRefs;
	/*
	// set up the pointers to the vectors. We need to sort the data - it's
	// easier to just move "pointers" around.
	*/
	for(k = 0; k < NumMaps; k++)
	{
		for(i=0; i < Maps[k]->yVDim; i++)
		{
			for(j=0; j < Maps[k]->xVDim; j++)
			{
				pTmpVecRef->pVec = & Maps[k]->Rows[i][j];
				pTmpVecRef ++;
			}
		}
	} 

	/*
	// map all the colours into the perception space.
	//
	// **NOTE** for RGB{A} textures we reorder the vector components 
	// so that alpha is placed at the end. This is for the situations
	// where alpha is always opaque, which means we can save quite a
	// number of calculations.
	//
	// Similarly YUV only uses the first 8 components.
	*/
	pTmpVecRef = pSrcVectRefs;

	
	for(i = NumSrcVectors; i > 0; i--)
	{
		PIXEL_VECT *pVec;

		pVec = pTmpVecRef->pVec;

		RawToPerceptionSpace(Metric, pVec->v, pVec->pv); 

		pTmpVecRef++;
	}/*end for i*/


	/*
	// Create the root of the search tree we will use later
	*/
	*ppSearchRoot = NEW(SearchTreeNode);

	/*
	// If this fails, get out of here
	*/
	if(ppSearchRoot == NULL)
	{
		/*
		// Clean up our other allocation
		*/
		free(pSrcVectRefs);
		return VQ_OUTOFMEMORY;
	}
	
	ppSearchRoot[0]->LeafRepIndex = 0; /*a safety precaution only*/

	/*
	// determine the number of dimensions we need to process
	// Note that the routines that use this value have been coded
	// such that they assume NumDims is divisible by 4.
	*/
	if(bDoingAlpha)
	{
		NumDims = 16; /*i.e. full VECLEN*/
	}
	else if(Format == FORMAT_YUV)
	{
		NumDims = 8; /*4 Y's, 2U's, and 2V's*/
	}
	else
	{
		NumDims = 12; /*4 x RGB*/
	}
	

	/*
	// Set the initial partition to be all the colours
	*/
	NumPartitions = 1;
	Parts[0].Start  = 0;
	Parts[0].Length = NumSrcVectors;

	Parts[0].Error  = 1.0f; /*This value doesn't really matter for the first one*/
	
	Parts[0].pThisNode = *ppSearchRoot;

	/*
	// Initialise Worst partition: this is to stop compiler warnings, and to trap errors
	*/
	WorstPartition = INT_MIN;

	/*
	// Keep iterating until we have created as many partitions as the number
	// of reps we require.. (unless the image is very simple: see later)
	*/
	while(NumPartitions < NumRepsRequired)
	{

		DEB_OUT "%d\n", NumPartitions);

		/*
		// find the worst 'scoring' partition
		*/
		WorstErrorFound = -1.0f; /*errors can't be negative*/

		for(i = 0; i < NumPartitions; i++)
		{
			if(WorstErrorFound < Parts[i].Error)
			{
				WorstErrorFound = Parts[i].Error;
				WorstPartition  = i;
			} 

		}/*end for*/
		

		/*
		// if we've mapped everything exactly, then stop trying to split
		// partitions...
		*/
		if(WorstErrorFound == 0.0f)
		{
			break;
		}

		DEB_OUT "Worst Part:%d Error:%f\n", WorstPartition, Parts[WorstPartition].Error);
		DEB_OUT "   Start:%d  Length %d\n", Parts[WorstPartition].Start, Parts[WorstPartition].Length);

		/*
		// Allocate children for the current search node
		//
		*/
		pLess =	NEW(SearchTreeNode);
		pMore = NEW(SearchTreeNode);

		/*
		// If we have exhausted the memory, then exit. The tree clean up code should
		// cope with this case, so we only need to free the vector list memory and the
		// either of the nodes that we didn't link in.
		*/
		if((pLess == NULL) || (pMore == NULL))
		{

			DEB_OUT "Out of mem at partition:%d\n", NumPartitions);

			if(pLess)
			{
				free(pLess);
			}
			if(pMore)
			{
				free(pMore);
			}
			/*
			// Clean up our other allocation
			*/
			free(pSrcVectRefs);

			return VQ_OUTOFMEMORY;
		}


		/*
		// just in case we run out of memory in the future, initialise enough
		// of the nodes so that the cleanup deallocation routine won't
		// choke.
		*/
		pLess->LeafRepIndex = 0;
		pMore->LeafRepIndex = 0;

		/*
		// Connect the nodes into the tree
		*/
		Parts[WorstPartition].pThisNode->pLess = pLess;
		Parts[WorstPartition].pThisNode->pMore = pMore;

		Parts[WorstPartition].pThisNode->LeafRepIndex = -1; /*mark that it's not a leaf*/

		/*
		// put the node pointers into the child partitions, 
		// the More/Less bit doesn't matter at the moment
		*/
		Parts[NumPartitions].pThisNode  = pLess;
		Parts[WorstPartition].pThisNode = pMore;

		/*
		// generate the principal axis for the partition
		*/
		pPartitionStart = pSrcVectRefs + Parts[WorstPartition].Start;
		
		GenerateAxis(pPartitionStart,  Parts[WorstPartition].Length,
						MainAxis,
						NumDims,
						SQSums, 
						Sums, 
						&WeightSum);

		/*
		// sort the vectors along the principal axis
		*/
		SortAlongAxis(pPartitionStart, Parts[WorstPartition].Length, NumDims, MainAxis);
		 
		/*
		// Find the "best" partitioning point
		*/
		FindPartition(pPartitionStart, &Parts[WorstPartition], &Parts[NumPartitions], 
						NumDims, SQSums, Sums, WeightSum);

		NumPartitions++;	
	}/*end while*/



	/*
	// generate the representative for each partition
	*/
	for(i = 0; i < NumPartitions; i++)
	{
		int Sum[VECLEN]; /*Sum of all values in a partition*/

		for(k = 0; k < VECLEN; k++)
		{
			Sum[k] = 0;
		}


		/*
		// step through all the vectors in this partition
		*/
		pPartitionStart = pSrcVectRefs + Parts[i].Start;

		for(j = Parts[i].Length; j > 0; j--)
		{
			PIXEL_VECT *pVec;

			pVec = pPartitionStart->pVec;

			for(k = 0; k < VECLEN; k++)
			{
				Sum[k] += pVec->v[k];
			}

			pPartitionStart ++;
		}

		/*
		// convert the Sum of values into a representative
		*/
		SumToRep(Sum, Parts[i].Length, Format, pReps+i);

		/*
		// Set the search node to be a leaf
		*/
		Parts[i].pThisNode->LeafRepIndex = i;
	}/*end for i*/


	/*
	// tidy up the "internal only" allocations
	*/
	free(pSrcVectRefs);


	return NumPartitions;
}


/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/*
//  Vector Assignment and Dithering Code
*/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/*
// Debug statistics
*/
#if GATHER_STATS
	int NumDistanceCalcs;
	int SetupCalcs;
#endif

/*********************************************************/
/*********************************************************/



/*********************************************************/
/*
//
// Build Search Tree
// Complete the building of the search tree, by adding splitting
// planes etc.
//
*/
/*********************************************************/

static void RecursiveBuildSearchTree(const PIXEL_VECT *pRepVectors,
							SearchTreeNode *pTree, 
										int TreeAverage[VECLEN])
{
	int Child1Av[VECLEN], Child2Av[VECLEN];
	int C1Dot, C2Dot;
	int i;

	/*
	// if this node is a child, simply return it's value as the average
	*/
	if(pTree->LeafRepIndex >= 0)
	{
		/*
		// sanity check
		*/
		ASSERT(pTree->LeafRepIndex < MAX_CODES)

		/*
		// get access to the rep
		*/
		pRepVectors += pTree->LeafRepIndex;
		for(i=0; i < VECLEN; i++)
		{
			TreeAverage[i] = pRepVectors->v[i];	
		}

	}
	else
	{
		/*
		// check that it's initialised to the correct value
		*/
		ASSERT(pTree->LeafRepIndex == -1)

		/*
		// process the children
		*/
		RecursiveBuildSearchTree(pRepVectors,
									pTree->pLess, 
									Child1Av);

		RecursiveBuildSearchTree(pRepVectors,
									pTree->pMore, 
									Child2Av);

		/*
		// Compute the average for the parent, and the difference for
		// the splitting axis, and the dot products
		*/
		C1Dot = 0;
		C2Dot = 0;

		for(i=0; i < VECLEN; i++)
		{
			/*
			// average with rounding -- actually I think the rounding should
			// alternate... Hmmm. I'll have to think about this.
			*/
			TreeAverage[i] = (Child1Av[i] + Child2Av[i] + 1) >> 1;

			/*
			// Compute the difference vector
			*/
			pTree->SplittingAxis[i] = Child1Av[i] - Child2Av[i];

			/*
			// And the dot products
			*/
			C1Dot += Child1Av[i] * pTree->SplittingAxis[i];
			C2Dot += Child2Av[i] * pTree->SplittingAxis[i]; 
		}/*end for*/



#if GATHER_STATS
	SetupCalcs += 2; /*this is about equivalent to 2 distance calcs*/
#endif

		/*
		// If we've got these the wrong way around, swap them
		*/
		if(C1Dot > C2Dot)
		{
			SearchTreeNode *pTmp;
			
			pTmp = pTree->pLess;
			pTree->pLess = pTree->pMore;
			pTree->pMore = pTmp;
		}

		/*
		// get the split value
		*/
		pTree->d = (C1Dot + C2Dot) / 2;
	}/*end if leaf/parent*/

}/*end function*/





/*
// For convience, put a wrapper around the recursive routine
*/
static void BuildSearchTree(const PIXEL_VECT *pRepVectors, 
										SearchTreeNode *pTree)
{

	int Dummy[VECLEN];

#if GATHER_STATS
	NumDistanceCalcs = 0;
	SetupCalcs = 0;
#endif
	/*
	// call the recursive routine
	*/
	RecursiveBuildSearchTree(pRepVectors, pTree, Dummy);
}



/*********************************************************
* Build an NxN List of nearest Neighbours.
*********************************************************/

typedef struct
{
	int OtherRep;
	int Distance;
}NeighbInfo;

static NeighbInfo NeighbourArray[MAX_CODES][MAX_CODES - 1];




/*********/

static int NeighComp(const void *pA, const void *pB)
{
	const NeighbInfo *pN1, *pN2;

	pN1 = pA;
	pN2 = pB;

	if(pN1->Distance > pN2->Distance)
	{
		return 1;
	}
	else if(pN1->Distance < pN2->Distance)
	{
		return -1;
	}
	else
	{
		return 0;
	}

}


static void	BuildNeighbourList(const PIXEL_VECT *pRepVectors,
								int NumReps)
{
	int i, j, k, dist;

	const PIXEL_VECT *pRepI, *pRepJ;
	/*
	// Step through all the rep vectors calculating the neighbour
	// distances
	*/
	pRepI = pRepVectors;
	for(i = 0; i < NumReps; i++)
	{
		pRepJ = pRepI + 1;
		for(j= i+1; j < NumReps; j++)
		{

			dist = 0;
			for(k = 0; k < VECLEN; k++)
			{
				dist += SQ(pRepI->v[k] - pRepJ->v[k]);
			}

#if GATHER_STATS
	SetupCalcs += 1;
#endif

			/*
			// store this in the two appropriate places
			*/
			NeighbourArray[i][j-1].OtherRep = j;
			NeighbourArray[i][j-1].Distance = dist;

			NeighbourArray[j][i].OtherRep = i;
			NeighbourArray[j][i].Distance = dist;

			pRepJ++;
		}/*end for j*/

		pRepI ++;
	}/*end for i*/

	/*
	// Sort each of these into increasing order of distance
	*/
	for(i = 0; i < NumReps; i++)
	{
		qsort(NeighbourArray[i], NumReps-1, sizeof(NeighbInfo),  NeighComp);
	}

}


/*********************************************************
* Find Closest Colour (using the Nearest Neighbour table)
*********************************************************/


/*
// Acceleration structure used to stop retesting of the same codes
*/
#define SIZE_ALL_READY_TESTED_BLOCK ((MAX_CODES + 31) / 32)



static int FindClosestVector(const int Vector[VECLEN],
							 const SearchTreeNode *pSearchRoot,

							 const PIXEL_VECT *pRepVectors,
					   		   int NumReps,
					   		 float *pErrorSum)
{
	int i, j;

	const PIXEL_VECT *pThisRep;
	int BestDistance, BestIndex, Dist;
	int CutoffDist;

	unsigned int Done[SIZE_ALL_READY_TESTED_BLOCK];
	/*
	// begin by using the tree structure to make an initial guess as to the
	// best candidate
	*/
	while(pSearchRoot->LeafRepIndex < 0)
	{
		int DotProd;
		/*
		// get the dot product with the splitting planes axis
		*/
		DotProd = 0;
		for(i=0; i < VECLEN; i++)
		{
			DotProd += Vector[i] * pSearchRoot->SplittingAxis[i];
		}

#if GATHER_STATS
	NumDistanceCalcs += 1; /*about equivalent to a distance calc*/
#endif

		if(DotProd <= pSearchRoot->d)
		{
			pSearchRoot= pSearchRoot->pLess;
		}
		else
		{
			pSearchRoot= pSearchRoot->pMore;
		}
	}/*end while*/

	
	BestIndex = pSearchRoot->LeafRepIndex;

	ASSERT(pSearchRoot->LeafRepIndex < NumReps)
	
	
	/*
	//Setup which ones we've already done
	*/
	for(i = 0; i < SIZE_ALL_READY_TESTED_BLOCK; i++)
	{ 
		Done[i] = 0;
	}

	/*
	// Mark off this one
	*/
	Done[BestIndex >> 5] |= (1 << (BestIndex &31));

	/*
	// measure the distance
	*/
	pThisRep = pRepVectors + BestIndex; 

	BestDistance = 0;
	for(i=0; i < VECLEN; i++)
	{
		BestDistance += SQ(Vector[i] - pThisRep->v[i]);
	}

#if GATHER_STATS
	NumDistanceCalcs += 1;
#endif

	/*
	// Compute the Cutoff distance. This is the half-way point,
	// but because we are using the squares of the distance, this
	// is actually 4* rather than 2*.
	*/
	CutoffDist = BestDistance * 4;

	/*
	// Check that this IS the best representative vector
	*/
	for(j = 0; j < NumReps - 1; j++)
	{
		int ThisNeighbour;

		/*
		// Can we trivially reject the next nearest neighbour (and hence
		// ALL other neighbours) 
		*/
		if(CutoffDist <= NeighbourArray[BestIndex][j].Distance)
		{
			/*
			// Eureka! we don't need to check any further
			*/
			break;
		}


		ThisNeighbour = NeighbourArray[BestIndex][j].OtherRep;
		/*
		// if we've already done this one
		*/
		if(Done[ThisNeighbour >> 5] & (1 << (ThisNeighbour &31)))
		{
			continue;
		}

		/*
		// Else see if we are actually closer to this neighbour
		*/
		pThisRep = pRepVectors + NeighbourArray[BestIndex][j].OtherRep;
		Dist = 0;
		for(i=0; i < VECLEN; i++)
		{
			Dist += SQ(Vector[i] - pThisRep->v[i]);
		}

#if GATHER_STATS
	NumDistanceCalcs += 1;
#endif
		/*
		// if this distance is BETTER than our current best, then
		// change our best
		*/
		if(Dist < BestDistance)
		{
			BestDistance = Dist;
			CutoffDist = BestDistance * 4;

			BestIndex = ThisNeighbour;

			/*
			// restart the for loop
			*/
			j = -1;
		}

		/*
		// else mark this one off as already done
		*/
		else
		{
			Done[ThisNeighbour >> 5] |= (1 << (ThisNeighbour &31));
		} /*end if this other rep is closer*/
	}/*end for testing the neighbours in order of distance*/

	/*
	// Sum up the errors
	*/
	*pErrorSum += BestDistance;

	#if CHECK_FAST_LOOKUP
	if(1)  /*this is the first value we actually try*/
	{
		BestDistance = 0;

		for(j = 0; j < VECLEN; j++)
		{
			BestDistance += SQ(pRepVectors[BestIndex].v[j] - Vector[j]);
		}


		for(i = 0; i < NumReps; i++)
		{
			/*
			// measure the distance
			*/
			Dist = 0;

			for(j = 0; j < VECLEN; j++)
			{
				Dist += SQ(pRepVectors->v[j] - Vector[j]);
			}

			if(Dist < BestDistance)
			{

				DEB_OUT "Error! Best distance NOT obtained (%d vs %d)\n", Dist, BestDistance );

				return i;
			}

			pRepVectors++;

		}/*end for i*/
	}
	#endif
	return BestIndex;
}




/*********************************************************
* Special case for the 1x1 MIP map level
*********************************************************/
static int SinglePixelFind(PIXEL_VECT *pVector,
							 const PIXEL_VECT *pRepVectors,
					   		   int NumReps)
{
	int i, j;

	int BestDistance, BestIndex, Dist;

	BestDistance = INT_MAX;
	BestIndex = INT_MAX; /*To stop compiler warnings and trap errors*/
	/*
	// Run through ALL of the reps. Since this is a "one off", it's not
	// significant.
	*/
	for(j = 0; j < NumReps; j++)
	{
		/*
		// measure the distance - BUT ONLY FOR THE FIRST PIXEL
		*/
		Dist = 0;
		for(i=0; i < MAX_COMPS_PER_PIXEL; i++)
		{
			Dist += SQ(pVector->v[i] - pRepVectors->v[i]);
		}
		pRepVectors ++;


		/*
		// if this distance is better
		*/
		if(Dist < BestDistance)
		{
			BestDistance = Dist;

			BestIndex = j;
		}
	}/*end for*/

	return BestIndex;
}


/*********************************************************/
/*********************************************************/
static float MapImageToIndices(IMAGE_VECTOR_STRUCT *Maps[MAX_MIP_LEVELS],
										int NumMaps,
						 		SearchTreeNode    *pSearchRoot,

					   const PIXEL_VECT *pRepVectors,
					   		              int NumReps,
					   	   SUM_USAGE_STRUCT   SumAndUsage[MAX_CODES],
									  int 	DiffusionLevel,
									  int	DitherJust1stComponent)
{
	IMAGE_VECTOR_STRUCT * pImage; /*current image*/
	/*
	// **Dithering**
	// We maintain two rows of pixel error values. One is the
	// errors from the previous row (i.e. the bottom pixel row of the
	// previous vector), the other are the error from the bottom
	// pixels of the "current" row of vectors.
	//
	// We alternate the error rows at each row of vectors.
	*/
	int VErrRow[2][MAX_X_PIXELS+1][MAX_COMPS_PER_PIXEL];
	int PrevRowIndex;
	int *pPreviousRow, *pCurrentRow;
	
	int HErr[PIXEL_BLOCK_SIZE][MAX_COMPS_PER_PIXEL];

	int x,y,i, Level;
	int DiffusionLimit;

	PIXEL_VECT *pVector;

	float Error;

	Error = 0.0f;

	/*
	// the following code ONLY works with a 2x2 pixel block
	*/
	ASSERT(PIXEL_BLOCK_SIZE==2)

	/*
	// initialise the usage count
	*/
	for(i = 0; i < NumReps; i++)
	{
		SumAndUsage[i].Usage = 0;
		for(x = 0; x < VECLEN; x++)
		{
		 	SumAndUsage[i].Sum[x] = 0;
		}
	}

   	/*
	// Complete the search tree for the initial Guess
	*/
	BuildSearchTree(pRepVectors, pSearchRoot);

	/*
	// create the NxN Nearest neighbour structure.
	*/
	BuildNeighbourList(pRepVectors, NumReps);


	/*
	// Step through each map level
	*/
	for(Level = 0; Level < NumMaps; Level++)
	{
		pImage = Maps[Level];


		/*
		// special case for the 1x1 Mip map level. We are only
		// interested in matching the top left pixel of the vector
		*/
		if((Level == (NumMaps - 1)) && pImage->xVDim == 1)
		{
			int Code;
			/*
			// Find the closest match
			*/
			Code = SinglePixelFind(pImage->Rows[0], pRepVectors, NumReps);
	    
			pImage->Rows[0][0].wc.Code = Code;


		
			SumAndUsage[Code].Usage++;

			for(i = 0; i < VECLEN; i++)
			{
				SumAndUsage[Code].Sum[i] += pImage->Rows[0][0].v[i];
			}
			break;			
		}


		/*
		// Intialise the error diffusion values for the 'previous row of pixels'
		// to be zero
		*/ 
		PrevRowIndex = 0;
	    
		pPreviousRow = VErrRow[PrevRowIndex][0];
		for(x = 0; x < pImage->xVDim*PIXEL_BLOCK_SIZE; x++)
		{
			for(i = 0; i < MAX_COMPS_PER_PIXEL; i++)
			{	
				pPreviousRow[i] = 0;
			}
	    
			pPreviousRow+= MAX_COMPS_PER_PIXEL;	
		}

		/*
		// if we only want to dither the first component, then set up the
		// loop limit so that the others aren't touched
		*/
		if(DitherJust1stComponent)
		{
			DiffusionLimit = 1;
		}
		else
		{
			DiffusionLimit = MAX_COMPS_PER_PIXEL;					
		}


		/*
		// Step through the image
		*/
		for(y = 0; y < pImage->yVDim; y++)
		{
			/*
			// get easy access to the previous rows vertical errors, and
			// the error for this row
			*/
			pPreviousRow = VErrRow[PrevRowIndex][0];
			pCurrentRow  = VErrRow[PrevRowIndex^1][0];
	    
   	   		PrevRowIndex ^= 1;
	    
			/*
			// clear the horizontal error...
			*/
			for(i = 0; i < MAX_COMPS_PER_PIXEL; i++)
			{
				HErr[0][i] = 0;
				HErr[1][i] = 0;
			}
	    
			/*
			// and reset the first output vertical error
			*/
			for(i = 0; i < MAX_COMPS_PER_PIXEL; i++)
			{	
				pCurrentRow[i] = 0;
			}
	    
	    
			/*
			// step through the row
			*/
			pVector = pImage->Rows[y];
			 
			for(x = 0; x < pImage->xVDim; x++)
			{
				int NewVector[VECLEN];
				int Code;
	    
	    
				/*
				// Put in the error from neighbouring vectors
				//
				// NEED TO DO SOME ASCII ART HERE
				//
				*/
				if(DiffusionLevel > 0)
				{
					/*
					// do error diffusion on only the required components
					*/
					for(i = 0; i < DiffusionLimit; i++)
					{
						int index;
	    
						/*
						// Top Left
						*/
						NewVector[i] = pVector->v[i] + HErr[0][i] + pPreviousRow[i];
	    
						CLAMP(NewVector[i], 0, 255);
	    
						/*
						// Top Right
						*/
						index = i+MAX_COMPS_PER_PIXEL;
						NewVector[index] = pVector->v[index] + pPreviousRow[index];
	    
						CLAMP(NewVector[index], 0, 255);
	    
						/*
						// Bottom Left
						*/
						index += MAX_COMPS_PER_PIXEL;
						NewVector[index] = pVector->v[index] + HErr[1][i];
					
						CLAMP(NewVector[index], 0, 255);
	    
						/*
						// Bottom Right (no error to accumulate)
						*/
						index += MAX_COMPS_PER_PIXEL;
	    
						NewVector[index] = pVector->v[index];
		
					}/*end for*/


					/*
					// for any remaining components, just copy the values
					*/
					for(/*nil*/; i < MAX_COMPS_PER_PIXEL; i++)
					{
						NewVector[i] = pVector->v[i];
						NewVector[i +   MAX_COMPS_PER_PIXEL] = pVector->v[i +   MAX_COMPS_PER_PIXEL];
						NewVector[i + 2*MAX_COMPS_PER_PIXEL] = pVector->v[i + 2*MAX_COMPS_PER_PIXEL];
						NewVector[i + 3*MAX_COMPS_PER_PIXEL] = pVector->v[i + 3*MAX_COMPS_PER_PIXEL];
					}
				}
	    
				/*
				// Else no error diffusion
				*/
				else
				{
					for(i = 0; i < VECLEN; i++)
					{
						NewVector[i] = pVector->v[i];
					}
				}
	    
	    
				/*
				// Find the closest match
				*/
				Code = FindClosestVector(NewVector, pSearchRoot, 
						pRepVectors, NumReps, &Error);
	    
				pVector->wc.Code = Code;
	    
				/*
				// keep track of the usage frequency, and sum of values
				*/
				SumAndUsage[Code].Usage++;

				for(i = 0; i < VECLEN; i++)
				{
					SumAndUsage[Code].Sum[i] += NewVector[i];
				}

	    
				if(DiffusionLevel > 0)
				{
					/*
					// Compute the error
					*/
					for(i = 0; i < VECLEN; i++)
					{
						NewVector[i] =  NewVector[i] - pRepVectors[Code].v[i];
	    
						/*
						// Damp the error - trying to correct too large an error
						// looks bad, so don't let it get out of hand.
						*/
						CLAMP(NewVector[i], -16, 16);
	    
						/*
						// should we halve the error
						*/
						if(DiffusionLevel == 1)
						{
							NewVector[i] = HALF(NewVector[i]);
						}
					}/*end for i*/
	    
	    
					/*
					// Distribute the error
					*/
					for(i = 0; i < MAX_COMPS_PER_PIXEL; i++)
					{
						int ThreeEighths;
						int OneQuarter;
						int ThreeQuarters;
						int index;
			  		#if 0
						/* 
						// NOTE: The following doesn't appear to improve anything -
						//  In fact it seems to make things worse, so leave it out
						// for the moment!
						*/
	    
						/*
						// Distribute the error from the top left to the errors
						// of the other pixels
						*/
						ThreeEighths = NewVector[i];
						OneQuarter =  NewVector[i] - 2 * ThreeEighths;
	    
						NewVector[i+MAX_COMPS_PER_PIXEL]   += ThreeEighths;
						NewVector[i+2*MAX_COMPS_PER_PIXEL] += ThreeEighths;
						NewVector[i+3*MAX_COMPS_PER_PIXEL] += OneQuarter;
					#endif
	    
						/*
						// Distribute top rights error
						*/
						index = i+MAX_COMPS_PER_PIXEL;
	    
						ThreeQuarters = THREE_4RS(NewVector[index]);
						OneQuarter    = NewVector[index] - ThreeQuarters;
	    
						HErr[0][i] = ThreeQuarters;
						HErr[1][i] = OneQuarter;
	    
						/*
						// Distribute the bottom lefts error
						*/
						index = i+2*MAX_COMPS_PER_PIXEL;
	    
						ThreeQuarters = THREE_4RS(NewVector[index]);
						OneQuarter = NewVector[index] - ThreeQuarters;
	    
						pCurrentRow[i] += ThreeQuarters;  /*NOTE addition here*/
						pCurrentRow[i+MAX_COMPS_PER_PIXEL]  = OneQuarter;
	    
	    
	    
						/*
						// Distribute the bottom rights error: 
						// i.e. to Right, Bottom, and Bottom Right.
						*/
						index = i+3*MAX_COMPS_PER_PIXEL;
	    
						ThreeEighths= THREE_8THS(NewVector[index]);
						OneQuarter =  NewVector[index] - 2*ThreeEighths;
	    
						HErr[1][i] += ThreeEighths;			  /*NOTE addition here*/
						pCurrentRow[i+MAX_COMPS_PER_PIXEL] += ThreeEighths;  /*NOTE addition here*/
						pCurrentRow[i+2*MAX_COMPS_PER_PIXEL] = OneQuarter;
					}/*end for i*/
				}/*end if error diffusion/dithering*/
	    
				/*
				// move along
				*/			
				pVector++;
	    
				/* 
				// Note that we are moving two pixels at a time
				*/
				pPreviousRow+= (2 * MAX_COMPS_PER_PIXEL);
				pCurrentRow += (2 * MAX_COMPS_PER_PIXEL);
	    
			}/*end for x*/	
		}/*end for y*/
	}/*end for level*/
#if GATHER_STATS
	{
		int NumVecs;
		printf("\n Search statistics\n");
		
		NumVecs =  (pImage->yVDim * pImage->xVDim);

		printf("Num vectors:%d    Average Searches PerVector %d  Plus Setup Costs %d\n", 		
			NumVecs,
			(NumDistanceCalcs +  NumVecs/2) / NumVecs,
			(NumDistanceCalcs + SetupCalcs + NumVecs/2) / NumVecs);
	}
#endif

	return Error;

}


/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/*
//  MIP map generation, VQ file formatting, and Code order optimisation.
*/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/


/*********************************************************/
/*
// 	Allocation and deallocation of image vector maps:
*/
/*********************************************************/


static IMAGE_VECTOR_STRUCT *AllocateVectorMap(int nWidth)
{
	int i;
	int VecsMaxX, VecsMaxY;
	IMAGE_VECTOR_STRUCT *pImageVecs;

	/*
	// Allocate the initial block
	*/
	pImageVecs = NEW(IMAGE_VECTOR_STRUCT);
	if(pImageVecs == NULL)
	{
		return NULL;
	}

	/*
	// Special case: If the width is 1 (i.e. 1x1 map), then double it's dimension.
	*/
	if(nWidth == 1)
	{
		nWidth = 2;
	}

	
	/*
	// Set up the dimensions
	*/
	VecsMaxX = nWidth / PIXEL_BLOCK_SIZE;
	VecsMaxY = nWidth / PIXEL_BLOCK_SIZE;

	pImageVecs->xVDim = VecsMaxX;
	pImageVecs->yVDim = VecsMaxY;

	/*
	// Allocate all of the rows
	*/
	for(i = 0; i < VecsMaxY; i++)
	{
		pImageVecs->Rows[i]	= malloc(sizeof(PIXEL_VECT) * VecsMaxX);

		if(pImageVecs->Rows[i] == NULL)
		{
			break;
		}

	}
	

	/*
	// if we failed an allocation, clean up
	*/
	if(i < VecsMaxY)
	{
		for(i = i - 1; i >= 0; i--)
		{
			free(pImageVecs->Rows[i]);
		}

		/*
		// free the parent as well
		*/
		free(pImageVecs);

		return NULL;
	}

	/*
	// Else it all went perfectly
	*/
	return pImageVecs;
}



static void FreeVectorMap(IMAGE_VECTOR_STRUCT *pImageVecs)
{
	int i;

	if(pImageVecs)
	{
		/*
		// free all the rows
		*/
		for(i = 0; i < pImageVecs->yVDim; i++)
		{
			free(pImageVecs->Rows[i]);
		}

		free(pImageVecs);
	}
}


/*********************************************************/
/*
// 	ConvertBitMapToVectors:
//
// Converts the raw RBG and Alpha stream into our internal
// vector format.
*/
/*********************************************************/

static void ConvertBitMapToVectors(const U8 *pRGB,
								   const U8 *pAlpha,

								   int BGROrder,
								   int bAlphaOn,
								   int bInvertAlpha,

							IMAGE_VECTOR_STRUCT *pImageVecs,
							int PixelWeight)
{
	int i, j;
	int xPos, yPos;
	int VecsMaxX, VecsMaxY;
	int PixelWidth;

	PIXEL_VECT *pVect;

	const U8 * pTmpRGB;


	/*
	// if we should invert the sense of the alpha, make it
	// easy to do so....
	*/
	if(bInvertAlpha)
	{
		bInvertAlpha = 0xFF;
	}


	/*
	//for convenience, get the dimensions
	*/
	VecsMaxX = 	pImageVecs->xVDim;
	VecsMaxY = 	pImageVecs->yVDim;

	PixelWidth = VecsMaxX * PIXEL_BLOCK_SIZE;

	for(yPos = 0; yPos < VecsMaxY; yPos++)
	{
		int yLower = yPos * PIXEL_BLOCK_SIZE; 

		/*
		// get the pointer to the row
		*/		
		pVect = pImageVecs->Rows[yPos];
	
		for(xPos = 0; xPos < VecsMaxX; xPos++)
		{
			int k =0;
			int xLower= xPos * PIXEL_BLOCK_SIZE;

			/*
			// Set the vectors weighting
			*/
			pVect->wc.Weight = PixelWeight;
			/*
			// Re-arrange the NxN pixels
			*/
			for(i = yLower; i < yLower + PIXEL_BLOCK_SIZE; i++)
			{
				for(j = xLower; j < xLower + PIXEL_BLOCK_SIZE; j++)
				{
					/*
					// get a pointer to the RGB bits of the pixel
					*/
					pTmpRGB = pRGB + (i * PixelWidth + j) * 3;

					if(BGROrder)
					{
						pVect->v[k]   =  pTmpRGB[2];
						pVect->v[k+1] =  pTmpRGB[1];
						pVect->v[k+2] =  pTmpRGB[0];
					}
					else
					{
						pVect->v[k]   =  pTmpRGB[0];
						pVect->v[k+1] =  pTmpRGB[1];
						pVect->v[k+2] =  pTmpRGB[2];
					}

					/*
					// Do the alpha, IF ANY
					*/
					if(bAlphaOn)
					{
						pVect->v[k+3] = (U8) (pAlpha[i * PixelWidth + j] ^ bInvertAlpha);
					}
					else
					{
					 	pVect->v[k+3] = 0xFF; /*Fully opaque*/
					}

					k+=MAX_COMPS_PER_PIXEL;
				}/*end for j*/
			}/*end for i*/

			/*
			// Move on to the next vector
			*/
			pVect++;
		}/*end for xPos*/ 
	
	}/*end for yPos*/ 

}/*end ConvertBitMapToVectors */



/*********************************************************/
/*
// 	GenerateMIPMapLevel
//
// Takes a map with defined vectors along with the next lower res map
// with uninitialised vectors, and computes the vecs for the lower map.
//
// It just uses a simple 2x2 filter kernel. There seems little point
// in using anything else because the compression is usually quite
// lossy.
*/
/*********************************************************/

static void GenerateMIPMapLevel(const IMAGE_VECTOR_STRUCT *pHigher,
						       IMAGE_VECTOR_STRUCT *pLower,
						       int	Weight)
{
	int k;
	int xLowerPos, yLowerPos, xloc, yloc;
	
	int VecsMaxX, VecsMaxY;
	PIXEL_VECT *pDstVect;
	const PIXEL_VECT *pSrcVect;

	int Av;

	/*
	// NOTE routine assumes blocksize is 2
	*/
	ASSERT(PIXEL_BLOCK_SIZE == 2)

	/*
	// Special case the lowest resolution map
	*/
	if(pHigher->xVDim == 1)
	{
		for(k = 0; k < MAX_COMPS_PER_PIXEL; k++ )
		{
			Av = pHigher->Rows[0][0].v[k] + 
				 pHigher->Rows[0][0].v[k+MAX_COMPS_PER_PIXEL] +
				 pHigher->Rows[0][0].v[k+MAX_COMPS_PER_PIXEL*2] +
				 pHigher->Rows[0][0].v[k+MAX_COMPS_PER_PIXEL*3];

			/*
			// divide with rounding
			*/
			Av = (Av + 2) >> 2;

			/*
			// write the average to ALL pixels in the vector;
			*/
			pLower->Rows[0][0].v[k] = (U8) Av;
			pLower->Rows[0][0].v[k+MAX_COMPS_PER_PIXEL] = 	 (U8) Av;
			pLower->Rows[0][0].v[k+MAX_COMPS_PER_PIXEL*2] = (U8) Av;
			pLower->Rows[0][0].v[k+MAX_COMPS_PER_PIXEL*3] = (U8) Av;

		}/*end for k*/

		/*
		// intialise it's weight as well
		*/
		pLower->Rows[0][0].wc.Weight = Weight;
		
	}		
	/*
	// else it's of a more typical size
	*/
	else
	{	
		/*
		// Check that the sizes match up
		*/
		ASSERT(pHigher->xVDim == pLower->xVDim * PIXEL_BLOCK_SIZE)

		/*
		// Step through all the lower res vectors
		*/
		VecsMaxX = pLower->xVDim;
		VecsMaxY = pLower->xVDim;

		for(yLowerPos = 0; yLowerPos < VecsMaxY; yLowerPos++)
		{
			int yHigher = yLowerPos * PIXEL_BLOCK_SIZE; 

			/*
			// get the pointer to the destiation row
			*/		
			pDstVect = pLower->Rows[yLowerPos];
	

			for(xLowerPos = 0; xLowerPos < VecsMaxX; xLowerPos++)
			{
				int xHigher= xLowerPos * PIXEL_BLOCK_SIZE;

				U8 *pVecEnt;

				/*
				// get a pointer to the first vector value. This will be
				// the red value of the first pixel. It's value will be derived
				// from all the red components etc...
				*/
				pVecEnt = pDstVect->v;

				/*
				// step through the pixels inside the vector
				*/
				for(yloc = 0; yloc < PIXEL_BLOCK_SIZE; yloc++)
				{
					/*
					// get access to the source vector for this
					// destination pixel
					*/
					pSrcVect = &pHigher->Rows[yHigher+yloc][xHigher];

					for(xloc = 0; xloc < PIXEL_BLOCK_SIZE; xloc++)
					{
						/*
						// Step through the colour components
						*/
						for(k = 0; k < MAX_COMPS_PER_PIXEL; k++ )
						{
							Av = pSrcVect->v[k] + 
				 				 pSrcVect->v[k+MAX_COMPS_PER_PIXEL] +
				 				 pSrcVect->v[k+MAX_COMPS_PER_PIXEL*2] +
				                 pSrcVect->v[k+MAX_COMPS_PER_PIXEL*3];

							/*
							// divide with rounding
							*/
							Av = (Av + 2) >> 2;

							/*
							// store it in the right place in the destination
							*/
							*pVecEnt = (U8) Av;

							/*
							// Move to the next value in the vector
							*/
							pVecEnt++;							
						}/*end for k*/

						/*
						// Move to the next Source vector, which should ne the next one
						// in the row.
						*/
						pSrcVect++;
					}/*end for x: Stepping through the various source vectors*/

				}/*end for y: Stepping through the various source vectors*/

				/*
				// Store the assigned weight for this map
				*/
				pDstVect->wc.Weight = Weight;

				/*
				// move to the next vector to generate
				*/
				pDstVect++;
			}/*end for xpos*/

		}/*end for ypos*/

	}/*end if then else*/

}

/*********************************************************/
/*
// 	ConvertToYUV
//
// Converts an RGB map level to YUV. Note that we don't want to
// use this routine on the 1x1 map, becaues that gets stored as
// RGB 565.
*/
/*********************************************************/

static void ConvertToYUV(IMAGE_VECTOR_STRUCT *pMap)
{
	int xPos, yPos;
	
	int VecsMaxX, VecsMaxY;
	int yloc;
	PIXEL_VECT *pVect;

	/*
	// NOTE routine assumes blocksize is 2
	*/
	ASSERT(PIXEL_BLOCK_SIZE == 2)


	/*
	// Step through all the vectors
	*/
	VecsMaxX = pMap->xVDim;
	VecsMaxY = pMap->xVDim;

	for(yPos = 0; yPos < VecsMaxY; yPos++)
	{
		/*
		// get the pointer to the row
		*/		
		pVect = pMap->Rows[yPos];
	

		for(xPos = 0; xPos < VecsMaxX; xPos++)
		{

			U8 *pVecEnt;

			float YUV[PIXEL_BLOCK_SIZE][3];

			/*
			// get a pointer to the first vector value.
			*/
			pVecEnt = pVect->v;

			/*
			// step through the 2 pairs of pixels in the vector
			*/
			for(yloc = 0; yloc < PIXEL_BLOCK_SIZE; yloc++)
			{
				float Scale = 1.0f / 187.0f;
				int tmp;
				/*
				// Do the pixel pair:
				// first compute raw YUV for both pixels
				*/
				YUV[0][0] = (pVecEnt[0] * 55.0f + pVecEnt[1] * 110.0f + pVecEnt[2] * 22.0f)* Scale;
				YUV[0][1] = (pVecEnt[0] *-32.0f + pVecEnt[1] * -64.0f + pVecEnt[2] * 96.0f)* Scale;
				YUV[0][2] = (pVecEnt[0] * 96.0f + pVecEnt[1] * -80.0f + pVecEnt[2] *-16.0f)* Scale;

				YUV[1][0] = (pVecEnt[4] * 55.0f + pVecEnt[5] * 110.0f + pVecEnt[6] * 22.0f)* Scale;
				YUV[1][1] = (pVecEnt[4] *-32.0f + pVecEnt[5] * -64.0f + pVecEnt[6] * 96.0f)* Scale;
				YUV[1][2] = (pVecEnt[4] * 96.0f + pVecEnt[5] * -80.0f + pVecEnt[6] *-16.0f)* Scale;


				/*
				// pack these back into the bytes. The individual Y's a stored, but the U and V
				// values are an average for the pixel pair.
				//
				// Note that we need to clamp U and V to the legal range
				// for bytes because the values can overflow.
				//
				// Y0
				*/
				pVecEnt[0] = (U8)  (YUV[0][0] + 0.5f); 
				
				/*
				// U average
				*/
				tmp = (int) ((YUV[0][1] + YUV[1][1]) * 0.5f + 128.0f);
				CLAMP(tmp, 0, 255);
				pVecEnt[1] = (U8) tmp;

				
				/*
				// Y1 and V average
				*/				
				pVecEnt[4] = (U8)  (YUV[1][0] + 0.5f);

				tmp = (int) ((YUV[0][2] + YUV[1][2]) * 0.5f + 128.0f);
				CLAMP(tmp, 0, 255);
				pVecEnt[5] = (U8) tmp;
				
				/*
				// for safety and ease of debugging, zero the other entries
				*/
				pVecEnt[2] = 0;
				pVecEnt[3] = 0;
				pVecEnt[6] = 0;
				pVecEnt[7] = 0;

				/*
				// Move to the next values in the vector
				*/
				pVecEnt+=8;							
			}/*end for y: Stepping through the various source vectors*/

			/*
			// move to the next vector to process.
			*/
			pVect++;
		}/*end for xpos*/

	}/*end for ypos*/

}


/*************************************************
//
// Write out short in big or little endian
//
*************************************************/

static void WriteShortMem( U8 *ucP, int nVal, int nEndian )
{
	if (nEndian == LITTLE_ENDIAN)
	{
		*ucP++ = (U8)( nVal & 0xff );
		*ucP++ = (U8)( nVal >> 8 );
	}
	else
	{
		*ucP++ = (U8)( nVal >> 8 );
		*ucP++ = (U8)( nVal & 0xff );
	}
}

/*************************************************
//
// DeTwiddle
//
// The textures are stored in twiddled format, i.e. X and Y
// index bits are alternated.
//
// This takes the linear position of a particular pixel in 
// the twiddled texture, and works out what it's x an y coordinates 
// are.
//
// This is not the most efficient way of doing it, but it's a
// completely insignificant part of the overall runtime.
//
*************************************************/

static void DeTwiddle( int addr, int *xparam, int *yparam )
{
	int p;
	int x,y;


	x = 0;
	y = 0;

	/*
	// check the 16 positions
	*/
	for(p = 16; p!=0 ; p --)
	{
		y >>= 1;
		x >>= 1;

		/*
		// distribute the next two MSBs to the y and x indices
		*/
		y = y | (addr & 1) << 15;
		addr >>= 1;
		x = x | (addr & 1) << 15;
		addr >>= 1;
	}

	*xparam = x;
	*yparam = y;
}

						
/******************************************************************************
 * From			: vqfmem.c
 * Title		: Vector Quantisation Compression Code
 * Author		: Julian Hodgson.
 * Created		: 17/03/1997
 * Modified		: 17/11/1999 Simon Fenney:
 *							Adapted to new compression algorithm.
 ******************************************************************************/
int WriteVqfMemory(		void *MemoryOut, 
						int nFormat,
						int AlphaSupplied, 

						int bWriteVQFHeader,

						IMAGE_VECTOR_STRUCT *Maps[MAX_MIP_LEVELS],
						int NumMaps,

						PIXEL_VECT Reps[MAX_CODES],
						int	nNumCodes,
						int Reorder[MAX_CODES])

{
	int			x=0, y=0, addr=0;
	int			i, j, lev;
	int			r, g, b, a;
	int			nTemp=0;
	PIXEL_VECT	*psVector;
	int			nHt=0;
	int			nWd=0;

	int			nEndian=0;
	int			map=0, size=0, code_book_size=0;
	int			code_out=0;
	U8 *ucPtr;

	int InvReorder[MAX_CODES];

 
#if HACK_SHOW_LOWER_MAPS
 	int Direction;
#endif

	/*
	// Write as if machine is little endian 
	*/
	nEndian = LITTLE_ENDIAN;

	ucPtr = ( U8 *) MemoryOut;


	/*
	// round the number of codes up to the next power 2 that we handle
	// as part of the VQF format.
	*/
	for(i = 8; i < nNumCodes; i <<= 1 )
	{
		/* keep doubling until we get enough codes */
	}
	nNumCodes = i;



	/*
	// if writing the header in VQF Format
	*/
	if(bWriteVQFHeader)
	{

		/* Two chars */
		*ucPtr++ = 'P';
		*ucPtr++ = 'V';
			
		/* map_type */
		switch( nFormat) 
		{
			case FORMAT_4444:
				map = 9;
				break;
    
			case FORMAT_1555:
				if ( AlphaSupplied)
					map = 8;
				else
					map = 6;
				break;
    
			case FORMAT_565:
				map = 7;
				break;

			case FORMAT_YUV:
				map = 10;
				break;

			//case FORMAT_BUMP:
			//	map = 11;
			//	break;
    
			default:
				return VQ_INVALID_PARAMETER;
		}
    
		if ( NumMaps > 1)
			map += 64;
		
		*ucPtr++ = (U8) map;
		
		/* map_size */
		switch ( Maps[0]->xVDim * PIXEL_BLOCK_SIZE) 
		{
		
			case 32:
				size = 0;
				break;
    
			case 64:
				size = 1;
				break;
    
			case 128:
				size = 2;
				break;
    
			case 256:
				size = 3;
				break;
    
			case 8:
				size = 4;
				break;
    
			case 16:
				size = 5;
				break;
    
			case 512:
				size = 6;
				break;
    
			case 1024:
				size = 7;
				break;
    
			default:
				return -1;
		}
		
		/* x-dim */
		*ucPtr++ = (U8) size;
		
		/* y-dim - x_dim ( bitwise) but since square, zero for now */
		*ucPtr++ = 0;
		
		switch ( nNumCodes)
		{
			case 8:
				code_book_size = 0;
				break;
    
			case 16:
				code_book_size = 1;
				break;
    
			case 32:
				code_book_size = 2;
				break;
    
			case 64:
				code_book_size = 3;
				break;
    
			case 128:
				code_book_size = 4;
				break;
    
			case 256:
				code_book_size = 5;
				break;
    
			default:
				return -2;
		}
    
		*ucPtr++ = (U8) code_book_size;
    
		/* Empty part of header */
		for(i = 0; i < 6; i++)
		{
			*ucPtr++ = 0;
		}


	}/*end if VQF format*/

	/*
	// write the codebook out 
	*/
	for(i = 0; i < nNumCodes; i++)
	{
		int t;
		
		/*
		// special case for YUV textures. The 1x1 texture is always RGB 565. 
		// For simplicity, the compressor has reserved the last code for this map level
		*/
		if((i == (nNumCodes-1)) && ( nFormat == FORMAT_YUV ) && (NumMaps > 1))
		{
			nFormat = FORMAT_565; 	
		}

		/*
		// Get a pointer to the "ith" vector, and then
		// allow for any reordering...
		*/
		psVector = Reps+Reorder[i];

		t = 0;
		
		while (t < 4)
		{
			U8 *v =NULL;

			switch (t)
			{

				/*
				// Allow for the twiddled arrangement of the values within
				// the code book entries
				*/
				case 0: 
					v = &psVector->v[0];
					break;
				case 1:
					v = &psVector->v[MAX_COMPS_PER_PIXEL*2];
					break;
				case 2:
					v = &psVector->v[MAX_COMPS_PER_PIXEL];
					break;

				case 3:
				default:
					v = &psVector->v[MAX_COMPS_PER_PIXEL*3];
					break;
			}

			r = v[0] >> (8 - BitDepths[nFormat][0]);
			g = v[1] >> (8 - BitDepths[nFormat][1]);
			b = v[2] >> (8 - BitDepths[nFormat][2]);
			a = v[3] >> (8 - BitDepths[nFormat][3]);

			switch (nFormat)
			{
				case FORMAT_4444:
					nTemp = (a << 12) | (r << 8) | (g << 4) | b;
					break;

				case FORMAT_565:
					nTemp = (r << 11) | (g << 5) | b;
					break;

				case FORMAT_1555:

					nTemp = (a << 15) | (r << 10) | (g << 5) | b;
					break;

				case FORMAT_YUV:

					nTemp = (r << 8) | g;
					break;

				default:
					return VQ_INVALID_PARAMETER;
			}

			WriteShortMem( ucPtr, nTemp, nEndian );	
			
			/* Advance pointer */
			ucPtr += 2;

			++t;
		}
	}


	/*
	// Generate the Inverse of the reordering of codes so that we can remap the
	// indices
	*/
	for(i = 0; i < nNumCodes; i++)
	{
		InvReorder[Reorder[i]] = i;
	}


	/*
	// Write out indices: These are given in the order 1x1, 2x2 etc..
	*/

#if HACK_SHOW_LOWER_MAPS
/*
// This is a HORRIBLE hack to allow easy display of the lower level MIP maps
// by copying them into the location where the higher map should be. 
*/
	Direction = -1;

	for(lev = NumMaps - 1; lev < NumMaps; lev+= Direction)
	{

		if(lev == 0)
		{
			Direction = 1;
			continue;	
		}
#else
	for(lev = NumMaps - 1; lev>=0; lev--)
	{
#endif
	 

		nHt = Maps[lev]->yVDim;
		nWd = Maps[lev]->xVDim;

	#if 0	
		if(nHt==0 || nWd==0)
		{
			nHt=1;
		  	nWd=1;
		}
	#endif

		addr = 0;
		for(i = 0; i < nHt; i++)
		{
			for(j = 0; j < nWd; j++)
			{
				DeTwiddle( addr, &x, &y );

				code_out = Maps[lev]->Rows[y][x].wc.Code;

				/*
				// Allow for the reordering
				*/
				code_out = InvReorder[code_out];

				/*
				// When we are using fewer than 256 codes, pack them all toward the top
				*/				
				code_out += 256 - nNumCodes;
				
				ASSERT( ( code_out >= 256 - nNumCodes) && ( code_out <= 255) );

				*ucPtr++ = (U8) code_out;  
				
				addr ++;
			}
		}
	}/*end for levels*/
	

	return VQ_OK;

}

/******************************************************************************/
/******************************************************************************/


/*************************************************
//
// OptimisePlacement
//
// This is possibly not necessary on Dreamcast/CLX but may be of use on other
// VQ implementations.
//
// The basic idea of this function is to try to improve the locality of accesses
// into the VQ code book for the particular image.
//
// The routine simply looks through the image indices, and counts the number of
// times each code is adjacent to each other code. It then uses a simple greedy
// algorithm to try to renumber the indices so that the most frequent pairings are
// numerically close, and hence close  each other address space.
//
//
// SPECIAL CASE NOTE: When we are doing YUV Mip mapped, the 1x1 texture is represented
// by a 565 code. To simplify the compressor, it will be assigned code number (nNumCodes-1).
// Luckily, because this particular doesn't get used anywhere but in the 1x1, it will always
// end up still mapped to (nNumCodes-1).
//
*************************************************/
typedef int NeighbCountType[MAX_CODES][MAX_CODES];

static void OptimisePlacement(const IMAGE_VECTOR_STRUCT *Maps[MAX_MIP_LEVELS],
					 	const	int NumMaps,
					  	const 	int nNumCodes,
								int Reorder[MAX_CODES])
{
	NeighbCountType *pNeighbCount;
	#define NC (*pNeighbCount)


	const IMAGE_VECTOR_STRUCT *pThisMap;
	int XDim, YDim;

	int ThisCode, OtherCode;
	PIXEL_VECT *pThisRow, *pRowAbove, *pRowBelow;

	int i, j, k;

	int BestPairCount;
	int BestPairI, BestPairJ;


	char InSet[MAX_CODES];
	int  NumDone;

	/*
	// if there is ONLY one code, then there is no point in doing this and,
	// furthermore, the later code will break.
	*/
	if(nNumCodes == 1)
	{
		Reorder[0] = 0;
		return;
	}

	/*
	// malloc the neighbour count structure
	*/
	pNeighbCount = NEW(NeighbCountType);

	/*
	// just in case our memory allocation fails, set up a default reordering
	*/
	if(!pNeighbCount)
	{
		for(i = 0; i < nNumCodes; i++)
		{
			Reorder[i] = i;	
		}
		return;
	}



	/*
	// intialise the neighbour count structure
	*/
	for(i = 0; i < nNumCodes; i++)
	{
		for(j = 0; j < nNumCodes; j++)
		{
			NC[i][j] = 0;	
		}

		/*
		// to make sure we get all the codes, and to make the coding easy,
		// initialise each code to be a trivial neighbour of itself
		*/
		//NC[i][i] = 1;
	}

	/*
	// step through the image levels  counting the neighbour pairings 
	// in the up/down left/right directions
	// We won't worry about the edges too much as it'll simplify the 
	// algorithm a bit.
	*/
	for(k = 0; k < NumMaps; k++)
	{
		pThisMap = Maps[k];
		XDim = pThisMap->xVDim;
		YDim = pThisMap->yVDim;

		/*
		// step through this image
		*/
		for(i = 1; i < YDim-1; i++)
		{
			pThisRow = pThisMap->Rows[i];
			pRowAbove= pThisMap->Rows[i-1];
			pRowBelow= pThisMap->Rows[i+1];

			for(j = 1; j < XDim-1; j++)
			{
				/*
				// get the code here
				*/
				ThisCode = pThisRow[j].wc.Code;

				/*
				// look up, and count it
				*/
				OtherCode = pRowAbove[j].wc.Code;
				if(OtherCode != ThisCode)
				{
					NC[OtherCode][ThisCode]++;
					NC[ThisCode][OtherCode]++;
				}

				/*
				// look down
				*/
				OtherCode = pRowBelow[j].wc.Code;
				if(OtherCode != ThisCode)
				{
					NC[OtherCode][ThisCode]++;
					NC[ThisCode][OtherCode]++;
				}

				/*
				// look left
				*/
				OtherCode = pThisRow[j-1].wc.Code;
				if(OtherCode != ThisCode)
				{
					NC[OtherCode][ThisCode]++;
					NC[ThisCode][OtherCode]++;
				}

 				/*
				// look right
				*/
				OtherCode = pThisRow[j+1].wc.Code;
				if(OtherCode != ThisCode)
				{
					NC[OtherCode][ThisCode]++;
					NC[ThisCode][OtherCode]++;
				}

			}/*end for j*/
		}/*end for i*/
	}/*end for stepping through the maps*/


	/*
	// intialise the number of codes we've done
	*/
	for(k = 0; k < MAX_CODES; k++)
	{
		InSet[k] = 0;
	}
	NumDone = 0;


	/*
	// Find the most frequent pairing. Note that because of the symmetry of the
	// matrix, we only have to check the upper triangle.
	*/
	BestPairCount = -1;
	BestPairI =    INT_MIN;	/*intialise to something that'll pick up errors*/
	BestPairJ =    INT_MIN;

	for(i = 0; i < nNumCodes; i++)
	{
		for(j = i+1; j < nNumCodes; j++)
		{
			if(NC[i][j] > BestPairCount)
			{
				BestPairCount = NC[i][j];
				BestPairI	= i;
				BestPairJ	= j;
			}
		}
	}/*end for i*/

	/*
	// assign these two codes to our list
	*/
	InSet[BestPairI] = 1;
	InSet[BestPairJ] = 1;

	Reorder[0] =BestPairI; 
	Reorder[1] =BestPairJ; 
	NumDone = 2;

	/*
	// now assign the remaing codes 1 at a time
	*/
	while(NumDone != nNumCodes)
	{
		BestPairCount = -1;
		BestPairI =    INT_MIN;	/*intialise to something that'll pick up errors*/

		/*
		// step though the remaining codes to see which is the best one to add
		*/
		for(i = 0; i < nNumCodes; i++)
		{
			/*
			// if i has not yet been done
			*/
			if(!InSet[i])
			{
				int LocalSum = 0;
				/*
				// Sum up this one's neighbours
				*/
				for(j = 0; j < nNumCodes; j++)
				{
					/*
					// if J is in the done set, then add the stats
					*/
					if(InSet[j])
					{
						LocalSum += NC[i][j];
					}
				}/*end for j*/

				/*
				// is this more frequent?
				*/
				if(LocalSum > BestPairCount)
				{
					BestPairCount = LocalSum;
					BestPairI = i;
				}
			}/*end if i not in the set*/
		}/*end for i*/

		/*
		// add this one to our set
		*/
		InSet[BestPairI] = 1;
		Reorder[NumDone] = BestPairI;

		NumDone++;
	}/*end while*/
	/*
	// release the temporary memory
	*/
	free(pNeighbCount);

}
						
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/*
//  The CreateVq Function:
*/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
extern int CreateVq(const void*	InputArrayRGB,
					const void*	InputArrayAlpha,
						  void*	OutputMemory, 

						int		BGROrder,		/*if data is BGR not RGB*/
						int		nWidth, 		/*Square textures only*/
						int 	Reserved0,
						int		bMipMap, 		/*Generate MIP map. RECOMMENDED!*/
						int		bAlphaOn, 		/*Is Alpha supplied?*/

						int 	bIncludeHeader,

				VQ_DITHER_TYPES	DitherLevel,	/* Level of dithering required */
				
						int		nNumCodes,		/*max number of codes requested*/
						int		nColourFormat,
										
						int		bInvertAlpha,	/*To support Old PVR1 files that had odd alpha */

						int 	Metric,			/*how to estimate colour differences*/
						int 	Reserved1,		/*Reserved. Set it to 0 */

						float	*pfErrorFound)
{


	IMAGE_VECTOR_STRUCT *Maps[MAX_MIP_LEVELS];
	SearchTreeNode * pSearchTree;

	int NumRepsNeeded = -1; /*initialise to rubbish to stop compiler warnings*/
	int NumMaps;

	int ReservedCodes;
	int SkipMaps;
	int DitherJust1stComponent;

	PIXEL_VECT Reps[MAX_CODES];
	SUM_USAGE_STRUCT SumAndUsage[MAX_CODES];

	float Errors[MAX_EXTRA_GLA_ITERATIONS]; /*the errors from the GLA passes*/
	
	int VectorCount;

	int		i, j;
	int		nReturnValue = VQ_OK;

	/*
	// the following is for optimising the order of codes for nearness in memory
	*/
	int Reorder[MAX_CODES];


#if DEBUG
	const U8 *bPtr;
	unsigned int CheckSum;
#endif


/*
// check that we've got the endianess #define correct
*/
#if DEBUG
	static union
	{
		long i;
		char c;
	}CheckEndian;
	
	CheckEndian.i = 1;

#if LITTLE_ENDIAN
	ASSERT(CheckEndian.c == 1);
#else
	ASSERT(CheckEndian.c == 0);
#endif

#endif


#if DEBUG
	FlatCount = 0;
#endif		



#if DEBUG_FILE
	DebFile = fopen("Debug.txt", "w");
	if(!DebFile)
	{
		DebFile = stdout;
	}
#endif

#if DEBUG
	CheckSum = 0;
	bPtr = InputArrayRGB;

	for(i = nWidth * nWidth * 3; i!= 0; i--)
	{
		/*
		// Do a rotate and flip a few bits
		*/
		CheckSum = ((CheckSum << 3) | (CheckSum >> (32-3))) ^ 0x101010;

		/*
		// Add in the current byte
		*/
		CheckSum += *bPtr;

		bPtr++;
	}

#endif



	/*
	// If 565 or YUV has been requested, then forcibly switch off alpha. No point
	// in wasting our time now, is there?
	*/
	if((nColourFormat == FORMAT_565) || (nColourFormat == FORMAT_YUV))
	{
		bAlphaOn = 0;
	}

	/*
	// Check the size of the texture
	*/
	switch(nWidth)
	{
		case 8:
		case 16:
		case 32:
		case 64:
		case 128:
		case 256:
		case 512:
		case 1024:
		{
			/* These are all ok */
			break;
		}
		default:
		{
			/*
			// This is rubbish
			*/
			return VQ_INVALID_SIZE;
		}

	}/*end switch*/


	/* Has the App malloced the memory yet? */
	if ( OutputMemory == NULL)
	{	
		/* Work out memory needed */
		int nMemoryNeeded = 0;


		if(bIncludeHeader)
		{
			nMemoryNeeded += 12;
		}/*end switch*/


		/* 
		// Allow space for code book
		*/
		nMemoryNeeded += 8 * nNumCodes;

		/*
		// Indices 
		//
		// Count the number needed for the lower mipmap levels
		// This is a lazy way to do it.... but it works :)
		*/
		if ( bMipMap)
		{
			/* Do 1x1 */
			nMemoryNeeded++;
			/* Start loop at 2x2 pix block = 1x1 code */
			for ( i=1; i < nWidth/2; i*=2)
			{
				nMemoryNeeded+= i*i;
			}
		}
		/* top one */
		nMemoryNeeded+= nWidth * nWidth / 4;

		/*
		// Exit function with number of bytes to allocate before coming
		// here again!
		*/
		return nMemoryNeeded;
	}


	/************************
	// We now assume that OutputMemory points to valid memory for "vqf" file (in memory)
	************************/

	/*
	// Initialise default number of reserved codes, and whether we should ignore 
	// particular maps to start with.
	*/
	ReservedCodes 	= 0;
	SkipMaps		= 0;
	DitherJust1stComponent = 0;


	/*
	// Allocate the memory for our vectorized data
	//
	// To make deallocation in an emergency easy, initialise everything to NULL
	*/
	for(i = 0; i < MAX_MIP_LEVELS; i++)
	{
		Maps[i] = NULL;
	}

	/*
	// initialise the tree structure to NULL as well
	*/
	pSearchTree = NULL;

	/*
	// Create the top level vectors map
	*/
	Maps[0] = AllocateVectorMap(nWidth);
	NumMaps = 1;

	if(Maps[0] == NULL)
	{
		nReturnValue = VQ_OUTOFMEMORY;	
		goto cleanup_and_exit;
	}

	/*
	// Slap in the data
	*/
	ConvertBitMapToVectors( InputArrayRGB,
							InputArrayAlpha,

							BGROrder,
							bAlphaOn,
							bInvertAlpha,

							Maps[0],
							Weights[0]);
	/*
	// If the user _wisely_ chose to do MIP mapping...
	*/
	if(bMipMap)
	{
		for(i = 1; (nWidth >> i) > 0; i++)
		{
			Maps[i] = AllocateVectorMap(nWidth >> i);

			if(Maps[i] == NULL)
			{
				nReturnValue = VQ_OUTOFMEMORY;	
				goto cleanup_and_exit;
			}


			NumMaps ++;

			/*
			// Convert the higher level map into the lower one
			*/
			GenerateMIPMapLevel(Maps[i-1], Maps[i], Weights[i]);

		}/*end for i*/
	}/*end if need Mip maps*/






	/*
	// if we are doing YUV, convert the format for all but the 1x1 texture from
	// RGB to YUV.
	// The 1x1 map is stored as RGB. To make life easy,we reserve a special code
	// for it.
	*/
	if(nColourFormat == FORMAT_YUV)
	{
		int FreqFlag;

		ConvertToYUV(Maps[0]);

		/*
		// If the weighted colour space metric was chosen, change to the equivalent
		// for YUV
		//
		// If we want frequency based weighting, then limit this to just the
		// Y component.
		*/		
		FreqFlag = Metric & VQMETRIC_FREQUENCY_FLAG;

		if((Metric & VQ_BASE_METRIC_MASK) != VQMetricEqual)
		{
			Metric = VQMetricWeightedYUV;
		}
		else
		{ 
			Metric = VQMetricEqual;
		}

		if(FreqFlag)
		{
			Metric |= VQMETRIC_FREQUENCY_FLAG_JUST_FIRST;
		}

		/*
		// only permit dithering/diffusion on the Y component
		*/ 
		DitherJust1stComponent = 1;

		/*
		// if we are mip mapping, then convert all bar the 1x1 map to
		// YUV. The 1x1 map gets its own code.
		*/
		if(bMipMap)
		{
			int Sum[VECLEN];
			for(i = 1; i < NumMaps - 1; i++)
			{
				ConvertToYUV(Maps[i]);
			}

			/*
			// reserve the extra code for the 1x1 case.
			*/
			ReservedCodes = 1;
			SkipMaps = 1;

			/*
			// Set up the reserved code...
			*/
			for(i = 0; i < VECLEN; i++)
			{
				Sum[i] = Maps[NumMaps-1]->Rows[0][0].v[i];
			}
			SumToRep(Sum, 1, FORMAT_565, Reps + (nNumCodes-1));

			/*
			// and point the vector at that code
			*/
			Maps[NumMaps-1]->Rows[0][0].wc.Code =  nNumCodes - 1;
		}/*end if MIP mapped*/

	}/*end if YUV format*/
	

	/*
	// Create the Representative Vectors
	*/
	NumRepsNeeded = VectorQuantizer(Maps, 
							NumMaps   - SkipMaps,
							nNumCodes - ReservedCodes,
							nColourFormat,
							bAlphaOn,
							Metric,
							&pSearchTree,		
							Reps,
							&VectorCount);


	if(NumRepsNeeded < 0)
	{
		nReturnValue = VQ_OUTOFMEMORY;	
		goto cleanup_and_exit;
	}


	/*
	// At this point we launch into a full-blown GLA (Generalised Lloyd's
	// Algorithm) as was used in the old VQ compressor. We don't do many
	// iterations as the gains rapidly become insignificant.
	*/
	for(j = EXTRA_GLA_ITS; j >= 0; j--)
	{
		int LocalDitherSetting;

		/*
		// Only use dithering on the last pass
		*/
		if(j == 0)
		{
			LocalDitherSetting = DitherLevel;
		}
		else
		{
			LocalDitherSetting = 0;
		}

		/*
		// Map the vectors to the representative set
		*/
		Errors[j] = MapImageToIndices(Maps, 
							NumMaps - SkipMaps,
					  		pSearchTree,
					  		Reps,
					  		NumRepsNeeded,
					  		SumAndUsage,
					  		LocalDitherSetting,
					  		DitherJust1stComponent);


		/*
		// Recompute the reps based on the vectors which really map to them
		//
		// THE FOLLOWING TEST IS CURRENTLY DISABLED:
		// Only do this when we aren't dithering, because I think the dithering
		// phase tends to mess this up a bit.
		*/
		if(1)//!LocalDitherSetting)
		{
			for(i = 0; i < NumRepsNeeded; i++)
			{
				if(SumAndUsage[i].Usage > 0)
				{
					SumToRep(SumAndUsage[i].Sum, SumAndUsage[i].Usage, 
						nColourFormat, Reps+i);
				}
			}/*end for i*/
		}
	}/*end for final GLA passes*/

	
	/*
	// Convert the total error into a per-component average
	*/
	if(bAlphaOn)
	{
		*pfErrorFound = (float) sqrt((float) Errors[0] / (VectorCount * VECLEN));
	}
	else
	{
		*pfErrorFound = (float) sqrt((float) Errors[0] / (VectorCount * 12 ));
	}



#if DEBUG
	/*
	// Output summary file: if you're running with VQGen, this gets written
	// to the source file's directory.						   ,
	*/
	{
		FILE *Summary;
		int i, j;
		Summary = fopen("CodeSummary.txt", "w");

		if(Summary)
		{
			fprintf(Summary, "Format:%3d Alpha:%3d  Dither:%3d\n\n", 
				nColourFormat, bAlphaOn, DitherLevel);
			//fprintf(Summary, "FlatCount = %d\n", FlatCount); 

			//fprintf(Summary, "RGB Data CheckSum: %x\n", CheckSum);


			fprintf(Summary, "GLA Errors:"); 
			for(j = EXTRA_GLA_ITS; j >= 0; j--)
			{
				fprintf(Summary, "Pass %d:%f  ", EXTRA_GLA_ITS - j, Errors[j]); 
			} 
			fprintf(Summary, "\n"); 


			for(i = 0; i< NumRepsNeeded; i++)
			{
				fprintf(Summary, "Code:%4d Usage:%6d  ", i, SumAndUsage[i].Usage);
				for(j = 0; j < VECLEN; j++)
				{
					if((j%4)==0)
					{
						fprintf(Summary, "   ");
					}
					fprintf(Summary, "%3d ", Reps[i].v[j]);
				} 
				fprintf(Summary, "\n");
			} 

			fclose(Summary);
		}
	}
#endif



	/*
	// On some architectures, jumping willy nilly about in the code book MIGHT introduce
	// page breaks or thrash a texture codebook cache. This routine attempts to reorder
	// the assignment of codes so that frequently occuring pairings are more likely
	// to be adjacent in memory
	//
	// On Dreamcast/CLX this routine is completely unnecessary, but it doesn't hurt
	// anyway.
	*/
	OptimisePlacement( 	Maps,
						NumMaps,
					  	nNumCodes,
						Reorder);


	/*
	// Write the results into the VQ memory format
	*/

	nReturnValue =  WriteVqfMemory(OutputMemory,
								nColourFormat,
								bAlphaOn, 
								bIncludeHeader,

								Maps,
								NumMaps,
								Reps,
								nNumCodes,
								Reorder);




/*
// Exit and Emergency exit point
*/
cleanup_and_exit:

#if DEBUG_FILE
	if(DebFile && (DebFile != stdout))
	{
		fclose(DebFile);
	}
#endif



   	/*
	// Release the image vector structures
	*/
	for(i = 0; i < MAX_MIP_LEVELS; i++)
	{
		FreeVectorMap(Maps[i]);
	}

	/*
	// release the search tree
	*/
	FreeSearchTree(pSearchTree);


	/*
	// if we had an error, report it
	*/
	if(nReturnValue < 0)
	{ 
		return nReturnValue;
	}
	/*
	// else return the number of codes we actually needed. This might allow
	// the user to recompress with fewer codes, if they want
	*/
	else
	{
		return  NumRepsNeeded + ReservedCodes;
	}
}


/*
// End of file
*/
