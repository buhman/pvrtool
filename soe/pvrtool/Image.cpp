/*************************************************
 Image Object
 
   The image object represents an in-memory
   (mipmapped) image. It also provides functions
   for loading and saving itself to/from disk.
   It also contains functions for displaying
   the image under Windows.

  To Do
  
    * Do horizontal and vertical flipping in
      a single pass


**************************************************/

#ifdef _WINDOWS
    #include <windows.h>
    #include <commctrl.h>
    #include "WinPVR/resource.h"
    #include "WinPVR/WinUtil.h"
#endif

#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "Image.h"
#include "Picture.h"
#include "Util.h"
#include "Resample.h"

extern const char* g_pszSupportedFormats[];


#ifdef _WINDOWS
//////////////////////////////////////////////////////////////////////
// Message processing function for the save options dialog
//////////////////////////////////////////////////////////////////////
BOOL CALLBACK DlgProc_SaveOptions( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    static SaveOptions* s_pSaveOption = NULL;
    switch( uMsg )
    {
        case WM_INITDIALOG:
        {
            //center the window over it's parent
            CenterWindow( hDlg, GetParent(hDlg) );

            //initialise controls
            s_pSaveOption = (SaveOptions*)lParam; 
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_ADDSTRING, 0, (LPARAM)"Smart" );
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_ADDSTRING, 0, (LPARAM)"565" );
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_ADDSTRING, 0, (LPARAM)"555" );
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_ADDSTRING, 0, (LPARAM)"1555" );
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_ADDSTRING, 0, (LPARAM)"4444" );
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_ADDSTRING, 0, (LPARAM)"Smart YUV" );
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_ADDSTRING, 0, (LPARAM)"YUV" );
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_ADDSTRING, 0, (LPARAM)"8888 (palette)" );
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_SETCURSEL, 0, 0 );

            SendDlgItemMessage( hDlg, IDC_PALETTEDEPTH, CB_ADDSTRING, 0, (LPARAM)"no palette" );
            SendDlgItemMessage( hDlg, IDC_PALETTEDEPTH, CB_ADDSTRING, 0, (LPARAM)"4 bpp" );
            SendDlgItemMessage( hDlg, IDC_PALETTEDEPTH, CB_ADDSTRING, 0, (LPARAM)"8 bpp" );

            //build INI file name
            char szINIFile[MAX_PATH+1];
            GetModuleFileName( NULL, szINIFile, MAX_PATH );
            ChangeFileExtension( szINIFile, "INI" );

            //load last used settings
            int iSel = GetPrivateProfileInt( "Save",     "ColourFormat", 0, szINIFile );
            int bTwiddle = GetPrivateProfileInt( "Save", "Twiddle",      1, szINIFile );
            int bMipMap = GetPrivateProfileInt( "Save",  "MipMap",       0, szINIFile );
            int bPad = GetPrivateProfileInt( "Save",     "Pad",          0, szINIFile );
            int iPalette = 0;
            if( s_pSaveOption->nPaletteDepth )
            {
                iPalette = s_pSaveOption->nPaletteDepth == 4 ? 1 : 2;
            }
            iPalette = GetPrivateProfileInt( "Save", "Palette",  iPalette, szINIFile );

            //plug these into the dialog
            SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_SETCURSEL, iSel, 0 );
            CheckDlgButton( hDlg, IDC_TWIDDLE, bTwiddle ? BST_CHECKED : BST_UNCHECKED );
            CheckDlgButton( hDlg, IDC_MIPMAPS, bMipMap ? BST_CHECKED : BST_UNCHECKED );
            CheckDlgButton( hDlg, IDC_PAD, bPad ? BST_CHECKED : BST_UNCHECKED );
            if( s_pSaveOption->nPaletteDepth )
            {
                SendDlgItemMessage( hDlg, IDC_PALETTEDEPTH, CB_SETCURSEL, 0, iPalette );
            }
            else
            {
                SendDlgItemMessage( hDlg, IDC_PALETTEDEPTH, CB_SETCURSEL, 0, 0 );
                EnableWindow( GetDlgItem( hDlg, IDC_PALETTEDEPTH ), FALSE );
            }
            break;
        }
            
        case WM_CLOSE:
            EndDialog( hDlg, IDCANCEL );
            break;

        case WM_COMMAND:
            switch( LOWORD(wParam) )
            {
                case IDOK:
                {
                    //extract options
                    s_pSaveOption->bMipmaps = ( IsDlgButtonChecked( hDlg, IDC_MIPMAPS ) == BST_CHECKED );
                    s_pSaveOption->bTwiddled = ( IsDlgButtonChecked( hDlg, IDC_TWIDDLE ) == BST_CHECKED );
                    s_pSaveOption->bPad = ( IsDlgButtonChecked( hDlg, IDC_PAD ) == BST_CHECKED );
                    BOOL bPal = FALSE;
                    if( s_pSaveOption->nPaletteDepth )
                    {
                        bPal = TRUE;
                        switch( SendDlgItemMessage( hDlg, IDC_PALETTEDEPTH, CB_GETCURSEL, 0, 0 ) )
                        {
                            default:
                            case 0: s_pSaveOption->nPaletteDepth = 0; break;
                            case 1: s_pSaveOption->nPaletteDepth = 4; break;
                            case 2: s_pSaveOption->nPaletteDepth = 8; break;
                        }
                    }
                    
                    int iSel = SendDlgItemMessage( hDlg, IDC_COLOURFORMAT, CB_GETCURSEL, 0, 0 );
                    switch( iSel )
                    {
                        case 0: s_pSaveOption->ColourFormat = ICF_SMART; break;
                        case 1: s_pSaveOption->ColourFormat = ICF_565; break;
                        case 2: s_pSaveOption->ColourFormat = ICF_555; break;
                        case 3: s_pSaveOption->ColourFormat = ICF_1555; break;
                        case 4: s_pSaveOption->ColourFormat = ICF_4444; break;
                        case 5: s_pSaveOption->ColourFormat = ICF_SMARTYUV; break;
                        case 6: s_pSaveOption->ColourFormat = ICF_YUV422; break;
                        case 7: s_pSaveOption->ColourFormat = ICF_8888; break;
                    }

                    //build INI file name
                    char szINIFile[MAX_PATH+1];
                    GetModuleFileName( NULL, szINIFile, MAX_PATH );
                    ChangeFileExtension( szINIFile, "INI" );

                    //save options
                    WritePrivateProfileInt( "Save", "ColourFormat", iSel, szINIFile );
                    WritePrivateProfileInt( "Save", "Twiddle", s_pSaveOption->bTwiddled, szINIFile );
                    WritePrivateProfileInt( "Save", "MipMap",  s_pSaveOption->bMipmaps, szINIFile );
                    WritePrivateProfileInt( "Save", "Pad", s_pSaveOption->bPad, szINIFile );
                    if( bPal ) WritePrivateProfileInt( "Save", "Palette", SendDlgItemMessage( hDlg, IDC_PALETTEDEPTH, CB_GETCURSEL, 0, 0 ), szINIFile );

                } //drop through
                case IDCANCEL:
                    EndDialog( hDlg, LOWORD(wParam) );
                    break;
            }
            break;
        
        default:
            return FALSE;
    }

    return TRUE;
}
#endif



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CImage::CImage()
{
#ifdef _WINDOWS
    m_fScaling = 1.0f;
    m_nMipMapLevel = 0;
    m_bChanged = false;
#endif

}

CImage::~CImage()
{
    Delete();
}


//////////////////////////////////////////////////////////////////////
// Image loading
//////////////////////////////////////////////////////////////////////
#ifdef _WINDOWS

bool CImage::PromptAndLoad( bool bLoadToAlphaChannel /*false*/ )
{
    /* build filter string */

    //calculate how much space we'll need
    int i, nSize = 0;
    for( i = 0; g_pszSupportedFormats[i] != NULL; i++ ) nSize += g_pszSupportedFormats[i] ? (strlen( g_pszSupportedFormats[i] ) + 1) : 0;
    nSize += 1024; //safe padding

    //allocate a buffer
    char* pszFilter = (char*)malloc( nSize );
    char* pszWrite = pszFilter;
    memset( pszFilter, 0, nSize );

    //write generic filter into buffer
    strcpy( pszWrite, "Supported Image Formats" ); pszWrite += strlen(pszWrite) + 1;
    for( i = 0; g_pszSupportedFormats[i] != NULL; i+=2 )
    {
        strcat( pszWrite, g_pszSupportedFormats[i] ); 
        strcat( pszWrite, ";" );
    }
    pszWrite += strlen(pszWrite) + 1;

    //write all others
    for( i = 0; g_pszSupportedFormats[i] != NULL; i+=2 )
    {
        strcpy( pszWrite, g_pszSupportedFormats[i+1] );  pszWrite += strlen(pszWrite) + 1;
        strcpy( pszWrite, g_pszSupportedFormats[i] );  pszWrite += strlen(pszWrite) + 1;
    }
    
    //add 'all files' option
    strcpy( pszWrite, "All Files (*.*)" );  pszWrite += strlen(pszWrite) + 1;
    strcpy( pszWrite, "*.*" );  pszWrite += strlen(pszWrite) + 1;



    /* get the file name */

    //prepare open file dialog
    static char s_szCustomFilter[1024] = "";
    static char s_szFilename[MAX_PATH] = "";
    OPENFILENAME ofn;
    ZeroMemory( &ofn, sizeof(ofn) );
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = pszFilter;//"Supported Image Formats (tga;bmp;pic;tif;pvr;vqf)\0*.tga;*.bmp;*.pic;*.pvr;*.vqf;*.tif;*.tiff\0All Files (*.*)\0*.*\0\0";
    ofn.lpstrCustomFilter = s_szCustomFilter;
    ofn.nMaxCustFilter = sizeof(s_szCustomFilter);
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = s_szFilename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_ENABLESIZING|OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "pvr";

    //display open file dialog
    if( GetOpenFileName( &ofn ) == false )
    {
        free( pszFilter );
        return false;
    }
    free( pszFilter );

    return Load( s_szFilename, bLoadToAlphaChannel );
}


bool CImage::PromptAndSave()
{
    //display save settings dialog
    SaveOptions options;
    if( DialogBoxParam( g_hInstance, MAKEINTRESOURCE(IDD_OUTPUTSETTINGS), g_hWnd, DlgProc_SaveOptions, (LPARAM)&options ) == IDCANCEL ) return false;

    /* get the file name */

    //prepare save file dialog
    static char s_szCustomFilter[1024] = "";
    static char s_szFilename[MAX_PATH] = "";
    OPENFILENAME ofn;
    ZeroMemory( &ofn, sizeof(ofn) );
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = "Supported Image Formats (.pvr;.c)\0*.pvr;*.c\0All Files (*.*)\0*.*\0\0";
    ofn.lpstrCustomFilter = s_szCustomFilter;
    ofn.nMaxCustFilter = sizeof(s_szCustomFilter);
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = s_szFilename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_ENABLESIZING|OFN_EXPLORER|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "pvr";

    //display open file dialog
    if( GetSaveFileName( &ofn ) == false ) return false;


    //get a pointer to the extension
    const char* pszExtension = GetFileExtension( s_szFilename );

    return Save( s_szFilename, &options );
}


//////////////////////////////////////////////////////////////////////
// Aligns the given RGB data on 32-byte boundaries
//////////////////////////////////////////////////////////////////////
unsigned char* CImage::AlignBitmap( unsigned char* pRawRGB, int nWidth, int nHeight, int nComponentsPerPixel )
{
    //calculate aligned width
    int nPitch = (long)(((long)(nWidth*nComponentsPerPixel*8) + 31) / 32) * 4;

    //allocate and initialise a new array
    unsigned char* pRGBAligned = (unsigned char*)malloc( nPitch * nHeight );
    memset( pRGBAligned, 0, nPitch * nHeight );

    //copy each line into the new array and return it to the caller
    for( int i = 0; i < nHeight; i++ )
        memcpy( &pRGBAligned[ i * nPitch ], &pRawRGB[ i * nWidth * nComponentsPerPixel ], nWidth * nComponentsPerPixel );

    return pRGBAligned;
}


//////////////////////////////////////////////////////////////////////
// Image to GDI Object and Image to Image conversion
//////////////////////////////////////////////////////////////////////
HBITMAP CImage::CreateBitmapFromRGB( unsigned char* pRGB, int nWidth, int nHeight )
{
    if( pRGB == NULL ) return NULL;

    //prepare bitmap info structure
    BITMAPINFOHEADER bih;
    ZeroMemory( &bih, sizeof(bih) );
    bih.biSize = sizeof(bih);
    bih.biWidth = nWidth;
    bih.biHeight = -nHeight;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    unsigned char* pBytes = AlignBitmap( pRGB, nWidth, nHeight, 3 );

    //create a display-compatible version of the RGB data
    HDC hDC = GetDC(NULL);
    HBITMAP hBitmap = CreateDIBitmap( hDC, &bih, CBM_INIT, (void*)pBytes, (BITMAPINFO*)&bih, DIB_RGB_COLORS );
    ReleaseDC( NULL, hDC );

    free( pBytes );

    //return the bitmap
    return hBitmap;
}

HBITMAP CImage::CreateBitmapFromAlpha( unsigned char* pAlpha, int nWidth, int nHeight )
{
    if( pAlpha == NULL ) return NULL;
    
    //prepare the header - use a custom structure with 256 palette entries
    struct { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[256]; } bi;
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = nWidth;
    bi.bmiHeader.biHeight = -nHeight;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 8;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biClrUsed = 256;
    bi.bmiHeader.biClrImportant = 256;

    //preapre the grayscale palette
    for( int i = 0; i < 256; i++ ) bi.bmiColors[i].rgbRed = bi.bmiColors[i].rgbGreen = bi.bmiColors[i].rgbBlue = i;

    unsigned char* pBytes = AlignBitmap( pAlpha, nWidth, nHeight, 1 );

    //create a display-compatible version of the RGB data
    HDC hDC = GetDC(NULL);
    HBITMAP hBitmap = CreateDIBitmap( hDC, &bi.bmiHeader, CBM_INIT, (void*)pAlpha, (BITMAPINFO*)&bi, DIB_RGB_COLORS );
    ReleaseDC( NULL, hDC );

    free( pBytes );

    //return the bitmap
    return hBitmap;
}


HBITMAP CImage::GetImageBitmap()
{
    if( m_mmrgbaAligned.pRGB == NULL || m_nMipMapLevel >= m_mmrgbaAligned.nMipMaps ) return NULL;
    return CreateBitmapFromRGB( m_mmrgbaAligned.pRGB[m_nMipMapLevel], m_mmrgbaAligned.nWidth >> m_nMipMapLevel, m_mmrgbaAligned.nHeight >> m_nMipMapLevel );
}

HBITMAP CImage::GetAlphaBitmap()
{
    if( m_mmrgbaAligned.pAlpha == NULL || m_nMipMapLevel >= m_mmrgbaAligned.nAlphaMipMaps ) return NULL;
    return CreateBitmapFromAlpha( m_mmrgbaAligned.pAlpha[m_nMipMapLevel], m_mmrgbaAligned.nWidth >> m_nMipMapLevel, m_mmrgbaAligned.nHeight >> m_nMipMapLevel );
}


//////////////////////////////////////////////////////////////////////
// Image display
//////////////////////////////////////////////////////////////////////
int CImage::GetScaledWidth()
{
    if( m_nMipMapLevel >= m_mmrgba.nMipMaps )
        return int(m_fScaling * m_mmrgba.nWidth);
    else
        return int(m_fScaling * (m_mmrgba.nWidth >> (g_bStretchMipmaps ? 0 : m_nMipMapLevel) ));
}

int CImage::GetScaledHeight() 
{ 
    if( m_nMipMapLevel >= m_mmrgba.nMipMaps )
        return int(m_fScaling * m_mmrgba.nHeight);
    else
        return int(m_fScaling * (m_mmrgba.nHeight >> (g_bStretchMipmaps ? 0 : m_nMipMapLevel) )); 
}


void CImage::Draw(HDC hDC, int x, int y )
{
    if( m_mmrgbaAligned.pRGB == NULL || m_nMipMapLevel >= m_mmrgbaAligned.nMipMaps  )
    {
        DrawFailMessage( hDC, x, y, "No Image" );
    }
    else
    {
        //prepare bitmap info structure
        BITMAPINFOHEADER bih;
        ZeroMemory( &bih, sizeof(bih) );
        bih.biSize = sizeof(bih);
        bih.biWidth = (m_mmrgba.nWidth >> m_nMipMapLevel);
        bih.biHeight = -( m_mmrgba.nHeight >> m_nMipMapLevel);
        bih.biPlanes = 1;
        bih.biBitCount = 24;
        bih.biCompression = BI_RGB;

        if( m_fScaling == 1.0f && (g_bStretchMipmaps == FALSE || m_nMipMapLevel == 0 ) )
        {
            //create an unscaled display-compatible version of the RGB data
            SetDIBitsToDevice( hDC, x, y, m_mmrgba.nWidth >> m_nMipMapLevel, m_mmrgba.nHeight >> m_nMipMapLevel, 0, 0, 0, -bih.biHeight, m_mmrgbaAligned.pRGB[m_nMipMapLevel], (LPBITMAPINFO)&bih, DIB_RGB_COLORS );
        }
        else
        {
            //calculate width & height
            int nDestW = m_mmrgba.nWidth, nDestH = m_mmrgba.nHeight;
            if( g_bStretchMipmaps == FALSE ) { nDestW >>= m_nMipMapLevel; nDestH >>= m_nMipMapLevel; }

            //transfer the image to the device
            StretchDIBits( hDC, x, y, int(nDestW*m_fScaling), int(nDestH*m_fScaling), 0, 0, bih.biWidth, -bih.biHeight, m_mmrgbaAligned.pRGB[m_nMipMapLevel], (LPBITMAPINFO)&bih, DIB_RGB_COLORS, SRCCOPY );
        }
    }
}

void CImage::DrawAlpha(HDC hDC, int x, int y )
{
    if( m_mmrgbaAligned.pAlpha == NULL || m_nMipMapLevel >= m_mmrgbaAligned.nAlphaMipMaps )
    {
        DrawFailMessage( hDC, x, y, "No Alpha" );
    }
    else
    {  
        //prepare the header - use a custom structure with 256 palette entries
        struct { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[256]; } bi;
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = (m_mmrgba.nWidth >> m_nMipMapLevel);
        bi.bmiHeader.biHeight = -( m_mmrgba.nHeight >> m_nMipMapLevel);
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 8;
        bi.bmiHeader.biCompression = BI_RGB;
        bi.bmiHeader.biClrUsed = 256;
        bi.bmiHeader.biClrImportant = 256;

        //prepare the grayscale palette
        for( int i = 0; i < 256; i++ ) bi.bmiColors[i].rgbRed = bi.bmiColors[i].rgbGreen = bi.bmiColors[i].rgbBlue = i;

        if( m_fScaling == 1.0f && (g_bStretchMipmaps == FALSE || m_nMipMapLevel == 0 ) )
        {
            //create an unscaled display-compatible version of the alpha data
            SetDIBitsToDevice( hDC, x, y, bi.bmiHeader.biWidth, -bi.bmiHeader.biHeight, 0, 0, 0, -bi.bmiHeader.biHeight, m_mmrgbaAligned.pAlpha[m_nMipMapLevel], (LPBITMAPINFO)&bi, DIB_RGB_COLORS );
        }
        else
        {
            //calculate width & height
            int nDestW = m_mmrgba.nWidth, nDestH = m_mmrgba.nHeight;
            if( g_bStretchMipmaps == FALSE ) { nDestW >>= m_nMipMapLevel; nDestH >>= m_nMipMapLevel; }

            //create a display-compatible version of the alpha data
            StretchDIBits( hDC, x, y, int(nDestW*m_fScaling), int(nDestH*m_fScaling), 0, 0, bi.bmiHeader.biWidth, -bi.bmiHeader.biHeight, m_mmrgbaAligned.pAlpha[m_nMipMapLevel], (LPBITMAPINFO)&bi, DIB_RGB_COLORS, SRCCOPY );
        }
    }
}

void CImage::DrawFailMessage( HDC hDC, int x, int y, const char *pszFailMessage )
{
    int nDestW = m_mmrgba.nWidth, nDestH = m_mmrgba.nHeight;
    if( g_bStretchMipmaps == FALSE ) { nDestW >>= m_nMipMapLevel; nDestH >>= m_nMipMapLevel; }
    RECT rc = { x, y, x + int(nDestW*m_fScaling), y + int(nDestH*m_fScaling) };
    FillRect( hDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH) );      
    HFONT hFontOld = (HFONT)SelectObject( hDC, GetStockObject(ANSI_VAR_FONT) );
    DrawText( hDC, pszFailMessage, -1, &rc, DT_LEFT|DT_NOPREFIX );
    SelectObject( hDC, hFontOld );
}

void CImage::SetDrawScaling( float fScaling /*1.0f*/ )
{
    m_fScaling = fScaling;
}
#endif


//////////////////////////////////////////////////////////////////////
// Mip Map display level modification
//////////////////////////////////////////////////////////////////////
#ifdef _WINDOWS
void CImage::SetMipMapLevel( int nMipMapLevel )
{
    if( nMipMapLevel < 0 ) nMipMapLevel = 0;
    int nMax = GetNumMipMaps();
    if( nMipMapLevel > nMax ) nMipMapLevel = nMax;

    if( m_nMipMapLevel != nMipMapLevel )
    {
        m_nMipMapLevel = nMipMapLevel;    
    }
}
#endif

int CImage::GetNumMipMaps()
{
    return m_mmrgba.nMipMaps;
}


//////////////////////////////////////////////////////////////////////
// Image initialisation and destruction
//////////////////////////////////////////////////////////////////////
void CImage::Delete()
{
    m_mmrgba.DeleteRGB();
    m_mmrgba.DeleteAlpha();
    m_mmrgba.nWidth = m_mmrgba.nHeight = 0;
    m_mmrgba.nMipMaps = m_mmrgba.nAlphaMipMaps = 0;

#ifdef _WINDOWS
    m_mmrgbaAligned.DeleteRGB();
    m_mmrgbaAligned.DeleteAlpha();
    m_mmrgbaAligned.nWidth = m_mmrgbaAligned.nHeight = 0;
    m_mmrgbaAligned.nMipMaps = m_mmrgbaAligned.nAlphaMipMaps = 0;
#endif
}

void CImage::CreateDefault()
{
    Delete();
    m_mmrgba.nWidth = m_mmrgba.nHeight = 128;
#ifdef _WINDOWS
    m_bChanged = false;
#endif
}



//////////////////////////////////////////////////////////////////////
// RGB To Alpha conversion (just uses mean of RGB)
//////////////////////////////////////////////////////////////////////
unsigned char* CImage::CreateAlphaFromRGB( unsigned char* pRGB, int nWidth, int nHeight )
{
    //prepare empty alpha channel
    unsigned char* pAlpha = (unsigned char*)malloc( nWidth*nHeight );
    memset( pAlpha, 0, nWidth*nHeight );

    //convert the RGB data to greyscale [average the RGB values]
    int iA = 0, iRGB = 0;
    for( int n = 0; n < nWidth*nHeight; n++ )
        pAlpha[iA++] = ( pRGB[iRGB++] + pRGB[iRGB++] + pRGB[iRGB++] ) / 3;

    //return it
    return pAlpha;
}






//////////////////////////////////////////////////////////////////////
// File Export
//////////////////////////////////////////////////////////////////////
#ifdef _WINDOWS
bool CImage::ExportFile()
#else
bool CImage::ExportFile( const char* s_szFilename )
#endif
{
    return false;
}



//////////////////////////////////////////////////////////////////////
// File Loading
//////////////////////////////////////////////////////////////////////
bool CImage::Load(const char *pszFilename, bool bLoadToAlphaChannel /*false*/ )
{
    /* turn on hourglass */
    IndicateLongOperation( true );

    /* load and generate the image */

    MMRGBA newmmrgba;

    //load in the mmrgba
    if( LoadPicture( pszFilename, newmmrgba, bLoadToAlphaChannel ? 0 : LPF_LOADALPHA ) )
    {
        if( bLoadToAlphaChannel )
        {
            //create alpha channel using RGB data
            if( newmmrgba.pAlpha ) free( newmmrgba.pAlpha );
            newmmrgba.pAlpha = NULL;
            if( newmmrgba.pRGB )
            {
                //make sure it's the same size
                if( m_mmrgba.nWidth != newmmrgba.nWidth || m_mmrgba.nHeight != newmmrgba.nHeight )
                {
                    ShowErrorMessage( "Load To Alpha: Different size for alpha image!" );
                    IndicateLongOperation( false );
                    return false;
                }

                //replace current mmrgba's alpha channel with this one
                m_mmrgba.AddAlpha();
                newmmrgba.ConvertTo32Bit(); //cheap hack - we convert it to 32 bit before we greyscale - rather a waste of time... could have dedicated method for palettised
                for( int iMipMap = 0; iMipMap < min(newmmrgba.nMipMaps,m_mmrgba.nMipMaps); iMipMap++ )
                {
                    m_mmrgba.pAlpha[iMipMap] = CreateAlphaFromRGB( newmmrgba.pRGB[iMipMap], newmmrgba.nWidth >> iMipMap, newmmrgba.nHeight >> iMipMap );
                }
            }
        }
        else
        {
            //replace the current mmrgba and regenerate the aligned image
            m_mmrgba.ReplaceWith( &newmmrgba );
        }
#ifdef _WINDOWS
        CreateAlignedImage();
#endif
    }
    else
    {
        IndicateLongOperation( false );
        return false;
    }

    IndicateLongOperation( false );
    return true;
}


//////////////////////////////////////////////////////////////////////
// File Saving
//////////////////////////////////////////////////////////////////////
bool CImage::Save( const char* pszFilename, SaveOptions* pSaveOptions )
{
    return SavePicture( pszFilename, m_mmrgba, pSaveOptions );
}

//////////////////////////////////////////////////////////////////////
// VQability
//////////////////////////////////////////////////////////////////////
bool CImage::CanVQ()
{
    if( m_mmrgba.nWidth != m_mmrgba.nHeight ) return false;
    switch( m_mmrgba.nWidth )
    {
        case 16:
        case 32:
        case 64:
        case 128:
        case 256:
        case 512:
        case 1024:
            return true;
        default:
            return false;
    }
}


#ifdef _WINDOWS
//////////////////////////////////////////////////////////////////////
// Generates the Windows-friendly aligned image
//////////////////////////////////////////////////////////////////////
void CImage::CreateAlignedImage()
{
    //delete existing
    m_mmrgbaAligned.Delete();

    //can't have an aligned image without an image...
    if( m_mmrgba.pRGB == NULL && m_mmrgba.pPaletteIndices == NULL )
    {
        m_mmrgbaAligned.nMipMaps = 0;
        m_mmrgbaAligned.nWidth = m_mmrgbaAligned.nHeight = 0;
        return;
    }
    
    //get image options and initialise
    bool bMipMaps = ( GetNumMipMaps() > 1);
    bool bAlpha = HasAlpha();
    bool bPalette = m_mmrgba.bPalette;
    m_mmrgbaAligned.Init( MMINIT_RGB|(bMipMaps?MMINIT_MIPMAP:0)|(bAlpha?MMINIT_ALPHA:0), m_mmrgba.nWidth, m_mmrgba.nHeight );

    if( bPalette )
    {
        //create 32-bit RGBA version of palettised image
        for( int i = 0; i < m_mmrgbaAligned.nMipMaps; i++ )
        {
            //build 32-bit image using palette
            int nSize = (m_mmrgba.nWidth >> i) * (m_mmrgba.nHeight >> i);
            unsigned char* pRGBWorkspace = (unsigned char*)malloc( nSize * 3 );
            unsigned char* pAlphaWorkspace = bAlpha ? (unsigned char*)malloc( nSize ) : NULL;
            for( int i2 = 0; i2 < nSize; i2++ )
            {
                pRGBWorkspace[(i2*3)+2] = m_mmrgba.Palette[ m_mmrgba.pPaletteIndices[i][i2] ].r;
                pRGBWorkspace[(i2*3)+1] = m_mmrgba.Palette[ m_mmrgba.pPaletteIndices[i][i2] ].g;
                pRGBWorkspace[(i2*3)  ] = m_mmrgba.Palette[ m_mmrgba.pPaletteIndices[i][i2] ].b;
                if( bAlpha ) pAlphaWorkspace[i2] = m_mmrgba.Palette[ m_mmrgba.pPaletteIndices[i][i2] ].a;               
            }

            //create aligned version
            m_mmrgbaAligned.pRGB[i] = AlignBitmap( pRGBWorkspace, m_mmrgba.nWidth >> i, m_mmrgba.nHeight >> i, 3 );
            if( bAlpha ) m_mmrgbaAligned.pAlpha[i] = AlignBitmap( pAlphaWorkspace, m_mmrgba.nWidth >> i, m_mmrgba.nHeight >> i, 1 );

            //free workspace
            free( pRGBWorkspace );
            free( pAlphaWorkspace );
        }
    }
    else
    {
        for( int i = 0; i < m_mmrgbaAligned.nMipMaps; i++ )
        {
            m_mmrgbaAligned.pRGB[i] = m_mmrgba.pRGB[i] ? AlignBitmap( m_mmrgba.pRGB[i], m_mmrgba.nWidth >> i, m_mmrgba.nHeight >> i, 3 ) : NULL;
            if( bAlpha ) m_mmrgbaAligned.pAlpha[i] = m_mmrgba.pAlpha[i] ? AlignBitmap( m_mmrgba.pAlpha[i], m_mmrgba.nWidth >> i, m_mmrgba.nHeight >> i, 1 ) : NULL;
        }
    }
}
#endif


//////////////////////////////////////////////////////////////////////
// Mip map generation
//////////////////////////////////////////////////////////////////////
void CImage::GenerateMipMaps()
{
    m_mmrgba.GenerateMipMaps();
    m_mmrgba.GenerateAlphaMipMaps();

#ifdef _WINDOWS
    CreateAlignedImage();
#endif
}


//////////////////////////////////////////////////////////////////////
// Deletes the image's mip maps
//////////////////////////////////////////////////////////////////////
void CImage::DeleteMipMaps()
{
    m_mmrgba.DeleteAllMipmaps();

#ifdef _WINDOWS
    m_mmrgbaAligned.DeleteAllMipmaps();
#endif
}

//////////////////////////////////////////////////////////////////////
// Deletes the image's alpha channel
//////////////////////////////////////////////////////////////////////
void CImage::DeleteAlpha()
{
    m_mmrgba.DeleteAlpha();
#ifdef _WINDOWS
    m_mmrgbaAligned.DeleteAlpha();
#endif
}


//////////////////////////////////////////////////////////////////////
// Enlarges the image to the nearest power of 2
//////////////////////////////////////////////////////////////////////
void CImage::EnlargeToPow2()
{
    //calculate the nearest power of 2 dimensions
    int nNewWidth = GetNearestPow2( m_mmrgba.nWidth ), nNewHeight = GetNearestPow2( m_mmrgba.nHeight );

    //enlarge the image
    Enlarge( nNewWidth, nNewHeight );
}

//////////////////////////////////////////////////////////////////////
// Enlarges the so that it is square
//////////////////////////////////////////////////////////////////////
void CImage::MakeSquare()
{
    if( m_mmrgba.nWidth != m_mmrgba.nHeight )
    {
        //get largest dimension
        int nDimension = max( m_mmrgba.nWidth, m_mmrgba.nHeight );

        //enlarge the image
        Enlarge( nDimension, nDimension );
    }
}



//////////////////////////////////////////////////////////////////////
// Flips the image
//////////////////////////////////////////////////////////////////////
void CImage::Flip( bool bHorizontal, bool bVertical )
{
    //validate parameters
    if( bHorizontal == false && bVertical == false ) return;

    //get image options
    bool bMipMaps = ( GetNumMipMaps() > 1);
    bool bAlpha = HasAlpha();

    //allocate a work area
    unsigned char* pWorkarea = (unsigned char*)malloc( max(m_mmrgba.nWidth, m_mmrgba.nHeight) * 3 );

    //do all mipmap levels
    for( int iMipmap = 0; iMipmap < GetNumMipMaps(); iMipmap++ )
    {
        int nTempWidth = m_mmrgba.nWidth >> iMipmap, nTempHeight = m_mmrgba.nHeight >> iMipmap;
        unsigned char* pRGB = m_mmrgba.pRGB ? m_mmrgba.pRGB[iMipmap] : NULL;
        unsigned char* pAlpha = ( bAlpha && m_mmrgba.pAlpha && m_mmrgba.pAlpha[iMipmap] ) ? m_mmrgba.pAlpha[iMipmap] : NULL;
        unsigned char* pPaletteIndices = m_mmrgba.pPaletteIndices ? m_mmrgba.pPaletteIndices[iMipmap] : NULL;

        //do vertical flip
        if( bVertical )
        {
            DisplayStatusMessage( "Vertical flip..." );
            for( int y = 0; y < nTempHeight/2; y++ )
            {
                if( pRGB )
                {
                    memcpy( pWorkarea,                             &pRGB[y*nTempWidth*3],                 nTempWidth*3 );
                    memcpy( &pRGB[y*nTempWidth*3],                 &pRGB[(nTempHeight-1-y)*nTempWidth*3], nTempWidth*3 );
                    memcpy( &pRGB[(nTempHeight-1-y)*nTempWidth*3], pWorkarea,                             nTempWidth*3 );
                }
                if( pAlpha )
                {
                    memcpy( pWorkarea,                             &pAlpha[y*nTempWidth],                 nTempWidth );
                    memcpy( &pAlpha[y*nTempWidth],                 &pAlpha[(nTempHeight-1-y)*nTempWidth], nTempWidth );
                    memcpy( &pAlpha[(nTempHeight-1-y)*nTempWidth], pWorkarea,                             nTempWidth );
                }
                if( pPaletteIndices )
                {
                    memcpy( pWorkarea,                             &pPaletteIndices[y*nTempWidth],                 nTempWidth );
                    memcpy( &pPaletteIndices[y*nTempWidth],        &pPaletteIndices[(nTempHeight-1-y)*nTempWidth], nTempWidth );
                    memcpy( &pPaletteIndices[(nTempHeight-1-y)*nTempWidth], pWorkarea,                             nTempWidth );
                }
            }
        }

        //do horiztontal flip
        if( bHorizontal )
        {
            DisplayStatusMessage( "Horizontal flip..." );
            unsigned char rgb[3], a, i;
            for( int x = 0; x < nTempWidth/2; x++ )
            {
                for( int y = 0; y < nTempHeight; y++ )
                {
                    if( pRGB )
                    {
                        memcpy( rgb,                                        &pRGB[((y*nTempWidth)+x)*3], 3 );
                        memcpy( &pRGB[((y*nTempWidth)+x)*3],                &pRGB[((y*nTempWidth)+(nTempWidth-1-x))*3], 3 );
                        memcpy( &pRGB[((y*nTempWidth)+(nTempWidth-1-x))*3], rgb, 3 );
                    }
                    if( pAlpha )
                    {
                        a = pAlpha[ (y*nTempWidth) + x ];
                        pAlpha[ (y*nTempWidth) + x ] = pAlpha[ (y*nTempWidth) + (nTempWidth-1-x) ];
                        pAlpha[ (y*nTempWidth) + (nTempWidth-1-x) ] = a;
                    }
                    if( pPaletteIndices )
                    {
                        i = pPaletteIndices[ (y*nTempWidth) + x ];
                        pPaletteIndices[ (y*nTempWidth) + x ] = pPaletteIndices[ (y*nTempWidth) + (nTempWidth-1-x) ];
                        pPaletteIndices[ (y*nTempWidth) + (nTempWidth-1-x) ] = i;
                    }
                    
                }
            }
        }
    }

    DisplayStatusMessage( "Done." );
    
    //clean up and generate the Windows-friendly version
    free( pWorkarea );
#ifdef _WINDOWS
    CreateAlignedImage();
#endif
}


//////////////////////////////////////////////////////////////////////
// Enlarges the image to the given size
//////////////////////////////////////////////////////////////////////
void CImage::Enlarge(int nNewWidth, int nNewHeight)
{
    //check parameters
    if( m_mmrgba.nWidth >= nNewWidth && m_mmrgba.nHeight >= nNewHeight ) return;

    //get image options
    bool bMipMaps = ( GetNumMipMaps() > 1);
    bool bAlpha = HasAlpha();
    bool bPalette = m_mmrgba.bPalette;

    //initialise the new image set
    MMRGBA mmrgba;
    mmrgba.Init( MMINIT_RGB|MMINIT_ALLOCATE|(bPalette?MMINIT_PALETTE:0)|(bMipMaps?MMINIT_MIPMAP:0)|(bAlpha?MMINIT_ALPHA:0), nNewWidth, nNewHeight );
    mmrgba.nPaletteDepth = m_mmrgba.nPaletteDepth;

    if( bPalette )
    {
        //copy palette
        memcpy( mmrgba.Palette, m_mmrgba.Palette, sizeof(MMRGBAPAL)*256 );

        //generate all mipmap levels
        for( int iMipmap = 0; iMipmap < GetNumMipMaps(); iMipmap++ )
        {
            int y;

            //copy it over one line at a time
            for( y = 0; y < m_mmrgba.nHeight; y++ )
            {
                //do copy
                memcpy( &mmrgba.pPaletteIndices[iMipmap][y*nNewWidth], &m_mmrgba.pPaletteIndices[iMipmap][y*m_mmrgba.nWidth], m_mmrgba.nWidth );

                //pad out rest of line with the last pixel colour
                int iOffsetLastPixel = (y*m_mmrgba.nWidth) + m_mmrgba.nWidth-1;
                for( int x = m_mmrgba.nWidth; x < mmrgba.nWidth; x++ )
                    mmrgba.pPaletteIndices[iMipmap][(y*nNewWidth+x)] = m_mmrgba.pPaletteIndices[iMipmap][iOffsetLastPixel];
            }

            //copy over the last line several times
            int iOffsetLastLine = (m_mmrgba.nHeight-1)*nNewWidth;
            for( y = m_mmrgba.nHeight; y < nNewHeight; y++ )
                memcpy( &mmrgba.pPaletteIndices[iMipmap][y*nNewWidth], &mmrgba.pPaletteIndices[iMipmap][iOffsetLastLine], mmrgba.nWidth );
        }   
    }
    else
    {
        //generate all mipmap levels
        for( int iMipmap = 0; iMipmap < GetNumMipMaps(); iMipmap++ )
        {
            int y;

            //copy it over one line at a time
            for( y = 0; y < m_mmrgba.nHeight; y++ )
            {
                //do copy
                memcpy( &mmrgba.pRGB[iMipmap][y*nNewWidth*3], &m_mmrgba.pRGB[iMipmap][y*m_mmrgba.nWidth*3], m_mmrgba.nWidth*3 );
                if( bAlpha ) memcpy( &mmrgba.pAlpha[iMipmap][y*nNewWidth], &m_mmrgba.pAlpha[iMipmap][y*m_mmrgba.nWidth], m_mmrgba.nWidth );

                //pad out rest of line with the last pixel colour
                int iOffsetLastPixel = (y*m_mmrgba.nWidth) + m_mmrgba.nWidth-1;
                for( int x = m_mmrgba.nWidth; x < mmrgba.nWidth; x++ )
                {
                    mmrgba.pRGB[iMipmap][((y*nNewWidth+x)*3)  ] = m_mmrgba.pRGB[iMipmap][(iOffsetLastPixel*3)  ];
                    mmrgba.pRGB[iMipmap][((y*nNewWidth+x)*3)+1] = m_mmrgba.pRGB[iMipmap][(iOffsetLastPixel*3)+1];
                    mmrgba.pRGB[iMipmap][((y*nNewWidth+x)*3)+2] = m_mmrgba.pRGB[iMipmap][(iOffsetLastPixel*3)+2];
                    if( bAlpha ) mmrgba.pAlpha[iMipmap][(y*nNewWidth+x)] = m_mmrgba.pAlpha[iMipmap][iOffsetLastPixel];
                }
            }

            //copy over the last line several times
            int iOffsetLastLine = (m_mmrgba.nHeight-1)*nNewWidth;
            for( y = m_mmrgba.nHeight; y < nNewHeight; y++ )
            {
                memcpy( &mmrgba.pRGB[iMipmap][y*nNewWidth*3], &mmrgba.pRGB[iMipmap][iOffsetLastLine*3], mmrgba.nWidth*3 );
                if( bAlpha ) memcpy( &mmrgba.pAlpha[iMipmap][y*nNewWidth], &mmrgba.pAlpha[iMipmap][iOffsetLastLine], mmrgba.nWidth );
            }
        }
    }

    //replace it
    strcpy( mmrgba.szDescription, m_mmrgba.szDescription );
    m_mmrgba.ReplaceWith( &mmrgba );

#ifdef _WINDOWS
    CreateAlignedImage();
#endif

}

void CImage::ScaleHalfSize()
{
    //make sure there's an image
    if( m_mmrgba.pRGB == NULL && m_mmrgba.pPaletteIndices == NULL ) return;

    //make sure the image is large enough to be resampled
    if( m_mmrgba.nWidth <= 8 || m_mmrgba.nHeight <= 8 )
    {
        DisplayStatusMessage( "Image is too small to resample" );
        return;
    }

    //make sure the image is an even width & height
    if( m_mmrgba.nWidth & 1 || m_mmrgba.nHeight & 1 )
    {
        DisplayStatusMessage( "Image width and height must be even numbers\n" );
        return;
    }

    //get image options
    bool bMipMaps = ( GetNumMipMaps() > 1 );
    bool bAlpha = HasAlpha();
    bool bPalette = m_mmrgba.bPalette;

    //prepare a new RGBA
    MMRGBA mmrgba;
    mmrgba.Init( MMINIT_RGB|MMINIT_ALLOCATE|(bPalette?MMINIT_PALETTE:0)|(bMipMaps?MMINIT_MIPMAP:0)|(bAlpha?MMINIT_ALPHA:0), m_mmrgba.nWidth / 2, m_mmrgba.nHeight / 2 );
    mmrgba.nPaletteDepth = m_mmrgba.nPaletteDepth;

    if( bPalette )
    {
        //copy palette
        memcpy( mmrgba.Palette, m_mmrgba.Palette, sizeof(MMRGBAPAL)*256 );

        //do all mipmap levels
        for( int iMipmap = 0; iMipmap < mmrgba.nMipMaps; iMipmap++ )
        {
            int nTempWidth = m_mmrgba.nWidth >> iMipmap, nTempHeight = m_mmrgba.nHeight >> iMipmap;

            //get the current pointers
            unsigned char* pPaletteIndices = m_mmrgba.pPaletteIndices[iMipmap];
            unsigned char* pNewPaletteIndices = mmrgba.pPaletteIndices[iMipmap];

            //resample them
            ResamplePalette( pNewPaletteIndices, pPaletteIndices, nTempWidth, nTempHeight, m_mmrgba.Palette );
        }
    }
    else
    {
        //do all mipmap levels
        for( int iMipmap = 0; iMipmap < mmrgba.nMipMaps; iMipmap++ )
        {
            int nTempWidth = m_mmrgba.nWidth >> iMipmap, nTempHeight = m_mmrgba.nHeight >> iMipmap;

            //get the current pointers
            unsigned char* pRGB = m_mmrgba.pRGB[iMipmap];
            unsigned char* pAlpha = ( bAlpha && m_mmrgba.pAlpha && m_mmrgba.pAlpha[iMipmap] ) ? m_mmrgba.pAlpha[iMipmap] : NULL;
            unsigned char* pNewRGB = mmrgba.pRGB[iMipmap];
            unsigned char* pNewAlpha = ( bAlpha && mmrgba.pAlpha && mmrgba.pAlpha[iMipmap] ) ? mmrgba.pAlpha[iMipmap] : NULL;

            //resample them
            ResampleRGB( pNewRGB, pRGB, nTempWidth, nTempHeight );
            if( pAlpha ) ResampleAlpha( pNewAlpha, pAlpha, nTempWidth, nTempHeight );
        }
    }

    //replace it
    strcpy( mmrgba.szDescription, m_mmrgba.szDescription );
    m_mmrgba.ReplaceWith( &mmrgba );

#ifdef _WINDOWS
    CreateAlignedImage();
#endif
}


void CImage::PageToMipmaps()
{
    //make sure there's an image
    if( m_mmrgba.pRGB == NULL && m_mmrgba.pPaletteIndices == NULL ) return;

    //check image
    if( m_mmrgba.nWidth != (m_mmrgba.nHeight >> 1) )
    {
        ShowErrorMessage( "Image must be 1:2 to create mipmaps from page" );
        return;
    }
    if( m_mmrgba.nMipMaps > 1 )
    {
        DisplayStatusMessage( "Deleting current mipmaps..." );
        DeleteMipMaps();
    }


    //get image options
    bool bAlpha = HasAlpha();
    bool bPalette = m_mmrgba.bPalette;

    //prepare a new RGBA
    MMRGBA mmrgba;
    mmrgba.Init( MMINIT_RGB|MMINIT_ALLOCATE|MMINIT_MIPMAP|(bPalette?MMINIT_PALETTE:0)|(bAlpha?MMINIT_ALPHA:0), m_mmrgba.nWidth, m_mmrgba.nWidth );
    mmrgba.nPaletteDepth = m_mmrgba.nPaletteDepth;

    if( bPalette )
    {
        //copy palette
        memcpy( mmrgba.Palette, m_mmrgba.Palette, sizeof(MMRGBAPAL)*256 );

        //do all mipmap levels
        unsigned char* pPaletteIndices = m_mmrgba.pPaletteIndices[0];
        for( int iMipmap = 0; iMipmap < mmrgba.nMipMaps; iMipmap++ )
        {
            unsigned char* pNewPaletteIndices = mmrgba.pPaletteIndices[iMipmap];
            for( int h = 0; h < (mmrgba.nHeight >> iMipmap); h++ )
            {
                int nTempWidth = (mmrgba.nWidth >> iMipmap);

                memcpy( pNewPaletteIndices, pPaletteIndices, nTempWidth );
                pNewPaletteIndices += nTempWidth;
                pPaletteIndices += mmrgba.nWidth;
            }
        }
    }
    else
    {
        //do all mipmap levels
        unsigned char* pRGB = m_mmrgba.pRGB[0];
        unsigned char* pAlpha = ( bAlpha && m_mmrgba.pAlpha && m_mmrgba.pAlpha[0] ) ? m_mmrgba.pAlpha[0] : NULL;
        for( int iMipmap = 0; iMipmap < mmrgba.nMipMaps; iMipmap++ )
        {
            unsigned char* pNewRGB = mmrgba.pRGB[iMipmap];
            unsigned char* pNewAlpha = ( bAlpha && mmrgba.pAlpha && mmrgba.pAlpha[iMipmap] ) ? mmrgba.pAlpha[iMipmap] : NULL;
            for( int h = 0; h < (mmrgba.nHeight >> iMipmap); h++ )
            {
                int nTempWidth = (mmrgba.nWidth >> iMipmap);

                memcpy( pNewRGB, pRGB, nTempWidth * 3 );
                if( pAlpha && pNewAlpha ) memcpy( pNewAlpha, pAlpha, nTempWidth );

                pNewRGB += (nTempWidth * 3);
                if( pNewAlpha ) pNewAlpha += nTempWidth;

                pRGB += ( mmrgba.nWidth * 3);
                if( pAlpha ) pAlpha += mmrgba.nWidth;
            }
        }
    }

    //replace it
    strcpy( mmrgba.szDescription, m_mmrgba.szDescription );
    m_mmrgba.ReplaceWith( &mmrgba );

#ifdef _WINDOWS
    CreateAlignedImage();
#endif

}

void CImage::MipmapsToPage()
{
    //make sure there's an image
    if( m_mmrgba.pRGB == NULL && m_mmrgba.pPaletteIndices == NULL ) return;

    //check image
    if( m_mmrgba.nWidth != m_mmrgba.nHeight )
    {
        ShowErrorMessage( "Image must be square to create page from mipmaps" );
        return;
    }
    if( m_mmrgba.nMipMaps <= 1 )
    {
        DisplayStatusMessage( "Generating mipmaps..." );
        GenerateMipMaps();
    }


    //get image options
    bool bAlpha = HasAlpha();
    bool bPalette = m_mmrgba.bPalette;

    //prepare a new RGBA
    MMRGBA mmrgba;
    mmrgba.Init( MMINIT_RGB|MMINIT_ALLOCATE|(bPalette?MMINIT_PALETTE:0)|(bAlpha?MMINIT_ALPHA:0), m_mmrgba.nWidth, m_mmrgba.nWidth << 1 );
    mmrgba.nPaletteDepth = m_mmrgba.nPaletteDepth;

    if( bPalette )
    {
        //copy palette
        memcpy( mmrgba.Palette, m_mmrgba.Palette, sizeof(MMRGBAPAL)*256 );

        //do all mipmap levels
        unsigned char* pNewPaletteIndices = mmrgba.pPaletteIndices[0];
        for( int iMipmap = 0; iMipmap < m_mmrgba.nMipMaps; iMipmap++ )
        {
            unsigned char* pPaletteIndices = m_mmrgba.pPaletteIndices[iMipmap];
            for( int h = 0; h < (m_mmrgba.nHeight >> iMipmap); h++ )
            {
                int nTempWidth = (m_mmrgba.nWidth >> iMipmap);

                memcpy( pNewPaletteIndices, pPaletteIndices, nTempWidth );
                pNewPaletteIndices += mmrgba.nWidth;
                pPaletteIndices += nTempWidth;
            }
        }
    }
    else
    {
        //do all mipmap levels
        unsigned char* pNewRGB = mmrgba.pRGB[0];
        unsigned char* pNewAlpha = ( bAlpha && mmrgba.pAlpha && mmrgba.pAlpha[0] ) ? mmrgba.pAlpha[0] : NULL;
        for( int iMipmap = 0; iMipmap < m_mmrgba.nMipMaps; iMipmap++ )
        {
            unsigned char* pRGB = m_mmrgba.pRGB[iMipmap];
            unsigned char* pAlpha = ( bAlpha && m_mmrgba.pAlpha && m_mmrgba.pAlpha[iMipmap] ) ? m_mmrgba.pAlpha[iMipmap] : NULL;
            for( int h = 0; h < (m_mmrgba.nHeight >> iMipmap); h++ )
            {
                int nTempWidth = (m_mmrgba.nWidth >> iMipmap);

                memcpy( pNewRGB, pRGB, nTempWidth * 3 );  if( pAlpha && pNewAlpha ) memcpy( pNewAlpha, pAlpha, nTempWidth );
                pNewRGB += (mmrgba.nWidth * 3);           if( pNewAlpha ) pNewAlpha += nTempWidth;
                pRGB += (nTempWidth * 3);                 if( pAlpha ) pAlpha += mmrgba.nWidth;
            }
        }
    }

    //replace it
    strcpy( mmrgba.szDescription, m_mmrgba.szDescription );
    m_mmrgba.ReplaceWith( &mmrgba );

#ifdef _WINDOWS
    CreateAlignedImage();
#endif

}
