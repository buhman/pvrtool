// Image.h: interface for the CImage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IMAGE_H__49968246_912E_11D3_86DB_005004314EE7__INCLUDED_)
#define AFX_IMAGE_H__49968246_912E_11D3_86DB_005004314EE7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Picture.h"

class CImage  
{
public:
	void MipmapsToPage();
	void PageToMipmaps();
	void MakeSquare();
	void Enlarge( int nWidth, int nHeight );
	void ScaleHalfSize();
	void Flip( bool bHorizontal, bool bVertical );
	void DeleteAlpha();

	CImage();
	virtual ~CImage();

    
    inline unsigned char* GetRGB() { return m_mmrgba.pRGB ? m_mmrgba.pRGB[0] : NULL ; };
    inline unsigned char* GetAlpha() { return m_mmrgba.pAlpha ? m_mmrgba.pAlpha[0] : NULL; }

	void EnlargeToPow2();
    void GenerateMipMaps();
    void DeleteMipMaps();
	int GetNumMipMaps();

    bool CanVQ();

#ifdef _WINDOWS
    virtual bool ExportFile();
    void SetMipMapLevel( int nMipMapLevel );

	void CreateAlignedImage();

	bool PromptAndLoad( bool bLoadToAlphaChannel = false );
    bool PromptAndSave();

    void SetDrawScaling( float fScaling = 1.0f );
	void Draw( HDC hDC, int x, int y );
	void DrawAlpha( HDC hDC, int x, int y );
    unsigned char* AlignBitmap( unsigned char* pRawRGB, int nWidth, int nHeight, int nComponentsPerPixel );
    int GetScaledWidth();
    int GetScaledHeight();
    bool m_bChanged;

    HBITMAP GetImageBitmap();
    HBITMAP GetAlphaBitmap();
    
#else
    virtual bool ExportFile( const char* s_szFilename );
#endif

	bool Load( const char* pszFilename, bool bLoadToAlphaChannel = false );
    bool Save( const char* pszFilename, SaveOptions* pSaveOptions );
	virtual void Delete();
    void CreateDefault();

    int GetWidth() { return m_mmrgba.nWidth; };
    int GetHeight() { return m_mmrgba.nHeight; };

    inline bool HasAlpha() { return (GetAlpha() != NULL); }

    inline MMRGBA* GetMMRGBA() { return &m_mmrgba; }

protected:
    unsigned char* CreateAlphaFromRGB( unsigned char* pRGB, int nWidth, int nHeight );


    MMRGBA m_mmrgba;

#ifdef _WINDOWS
    MMRGBA m_mmrgbaAligned;
	void DrawFailMessage( HDC hDC, int x, int y, const char *pszFailMessage );
    HBITMAP CreateBitmapFromRGB( unsigned char* pRGB, int nWidth, int nHeight );
    HBITMAP CreateBitmapFromAlpha( unsigned char* pAlpha, int nWidth, int nHeight );
    
    unsigned char* m_pRGBAligned;
    unsigned char* m_pAlphaAligned;

    float m_fScaling;
    
    int m_nMipMapLevel;
#endif
};

#endif // !defined(AFX_IMAGE_H__49968246_912E_11D3_86DB_005004314EE7__INCLUDED_)
