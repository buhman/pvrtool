/******************************************************************************
 * Name			: VQCALC.H
 * Title		: Vector Quantisation Compression Code
 * Author		: Julian Hodgson, Simon Fenney and Rowland Fraser
 *				  
 * Created		: 17/03/1997 
 *
 * Modified     : 16/11/1999 by Simon Fenney
 *						Completely new compression method - unneeded parameters have
 *						been removed.
 *
 *				: 14/7/1999 by Carlos Sarria.
 *
 * Copyright	: 1999 by Imagination Technologies Limited. All rights reserved.
 *				: No part of this software, either material or conceptual 
 *				: may be copied or distributed, transmitted, transcribed,
 *				: stored in a retrieval system or translated into any 
 *				: human or computer language in any form by any means,
 *				: electronic, mechanical, manual or other-wise, or 
 *				: disclosed to third parties without the express written
 *				: permission of Imagination Technologies Limited, Unit 8, HomePark
 *				: Industrial Estate, King's Langley, Hertfordshire,
 *				: WD4 8LZ, U.K.
 *
 * Description	: Code for Vector Quantisation Compression of a 2D image.
 *
 *				
 * History:		$Log: vqcalc.h,v $
 * Revision 1.2  2000/01/06  15:18:07  sjf
 * Improved the comments in the file.
 * 
 *
 * Platform		: ANSI (HS4/CLX2 Set 2.24)
 ******************************************************************************/

/*
// **NOTE**
//
//  Many of these are duplicated in vqdll.h.
//
//  MAKE SURE THEY STAY SYNCHRONISED
*/

/* Return messages */
#define VQ_OK			0
#define VQ_OUTOFMEMORY	-1
#define VQ_INVALID_SIZE -2
#define VQ_INVALID_PARAMETER -3

/*
// set output format
*/
#define	FORMAT_4444 (0)   /* The BEST mode for quality transparency*/
#define	FORMAT_1555 (1)	  /*This also doubles for the opaque 555 format*/
#define	FORMAT_555  (FORMAT_1555)
#define FORMAT_565	(2)

#define FORMAT_YUV	(3)
//#define FORMAT_BUMP	(4)



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
// to form a variation on the colour metric. it makes the 
// compressor tolerate greater inaccuracy in high frequency
// changes.
*/
#define VQMETRIC_FREQUENCY_FLAG 0x10
#define VQ_BASE_METRIC_MASK     0x0F


typedef enum
{
 	VQNoDither = 0,
	VQSubtleDither,  /*50% error diffusion*/
	VQFullDither
} VQ_DITHER_TYPES;



/******************************************************************************/
/*  THE ONLY EXTERN FUNTION                                                   */
/******************************************************************************/
extern int CreateVq(const void*	InputArrayRGB,
					const void*	InputArrayAlpha,
						  void*	OutputMemory, 

						int		BGROrder,		/*if data is BGR not RGB*/
						int		nWidth, 		/*Square textures only*/
						int		Reserved1,
						int		bMipMap, 		/*Generate MIP map. RECOMMENDED!*/
						int		bAlphaOn, 		/*Is Alpha supplied?*/

						int		IncludeHeader,	 /*optionally include the 12byte VQF header*/

				VQ_DITHER_TYPES	DitherLevel,	/* Level of dithering required */
				
						int		nNumCodes,		/*max number of codes requested*/
						int		nColourFormat,
										
						int		bInvertAlpha,	/*To support Old PVR1 files that had odd alpha */

						int 	Metric, 		/*a VQ_COLOUR_METRIC value optionally 
													ored with	frequency flag*/
						int 	Reserved2,		/*Reserved. Set it to 0 */

						float	*fErrorFound);







/*
// END OF FILE
*/
