// VQImage.h: interface for the CVQImage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_VQIMAGE_H__29F9C1C3_968F_11D3_86DB_005004314EE7__INCLUDED_)
#define AFX_VQIMAGE_H__29F9C1C3_968F_11D3_86DB_005004314EE7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Image.h"
#include "Picture.h"
#include "VQF.h"

class CVQImage : public CImage  
{
public:
	bool SaveAsC( const char* pszFilename );
	bool SaveAsPVR( const char* pszFilename );
	bool SaveAsVQF( const char* pszFilename );
	void SetVQ( unsigned char *pNewVQ, int nVQSize, int nCodebookSize, int nWidth, ImageColourFormat icf, bool bMipmap );
	CVQImage();
	virtual ~CVQImage();

    ImageColourFormat m_icf;

#ifdef _WINDOWS
    virtual bool ExportFile();
#else
    virtual bool ExportFile( const char* s_szFilename );
#endif

	virtual void Delete();

    bool UncompressVQ();

    inline unsigned char* GetVQ() { return m_pVQ; };
    inline int GetCodebookSize()  { return m_nVQCodebookSize; };

protected:
    unsigned char* m_pVQ;
    int m_nVQSize;
    int m_nVQWidth;
    int m_nVQCodebookSize;
    ImageColourFormat m_icfVQ;
    bool m_bVQMipmap;

};

#endif // !defined(AFX_VQIMAGE_H__29F9C1C3_968F_11D3_86DB_005004314EE7__INCLUDED_)
