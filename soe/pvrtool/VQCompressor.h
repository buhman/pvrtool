// VQCompressor.h: interface for the CVQCompressor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_VQCOMPRESSOR_H__29F9C1C2_968F_11D3_86DB_005004314EE7__INCLUDED_)
#define AFX_VQCOMPRESSOR_H__29F9C1C2_968F_11D3_86DB_005004314EE7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "VQImage.h"

extern "C" {
#include "vqdll.h"
}

class CVQCompressor
{
public:
	CVQCompressor();
	virtual ~CVQCompressor();

    CVQImage* GenerateVQ( CImage* pImage );

    ImageColourFormat m_icf;

    bool m_bMipmap;
    bool m_bTolerateHigherFrequency;
    int m_nCodeBookSize;
    VQ_DITHER_TYPES m_Dither;
    VQ_COLOUR_METRIC m_Metric;
};

#endif // !defined(AFX_VQCOMPRESSOR_H__29F9C1C2_968F_11D3_86DB_005004314EE7__INCLUDED_)
