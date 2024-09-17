/*************************************************
 Command Line Version of the PVR Tool


**************************************************/

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "max_path.h"
#include "stricmp.h"
#include "Picture.h"
#include "Image.h"
#include "VQImage.h"
#include "VQCompressor.h"
#include "CommandLineProcessor.h"
#include "Util.h"
#include "PVR.h"
#include "Twiddle.h"

#ifdef _DEBUG
    #include <assert.h>
    #include <conio.h>
#endif


const char* szApplication = "PVR Texture Conversion Command Line Tool";
const char* szVersion = "0.1.14 BETA";
const char* szOtherInfo = "Sega Europe ( EDTS@soe.sega.co.uk )\nContains paintlib [copyright (c) 1996-1998 Ulrich von Zadow]";


extern const char* g_pszSupportedFormats[];


//////////////////////////////////////////////////////////////////////
// Globals
//////////////////////////////////////////////////////////////////////
const char * g_pszAlphaFilename;
const char * g_pszAlphaPrefix;
const char * g_pszOutputExtension;
const char * g_pszOutputPath;
CImage g_Image;
CVQCompressor g_VQCompressor;

unsigned long int g_nGlobalIndex = 1;
bool g_bEnableGlobalIndex = false;

bool g_bVQCompress = false;
bool g_bBatchMipmap = false;
bool g_bEnlargeToPow2 = false;
bool g_bHFlip = false, g_bVFlip = false;
bool g_bHalfSize = false;
bool g_bMakeSquare = false;
bool g_bPagedMipmap = false;

SaveOptions g_SaveOptions;

int g_nFailed = 0;
int g_nSucceeded = 0;




//////////////////////////////////////////////////////////////////////
// Displays the current program options
//////////////////////////////////////////////////////////////////////
void DisplayParameters()
{
    if( *g_pszOutputPath ) printf( "Output path: %s\n", g_pszOutputPath );
    if( *g_pszAlphaFilename ) printf( "Alpha filename: %s\n", g_pszAlphaFilename );
    printf( "Twiddle %s\n", g_SaveOptions.bTwiddled ? "on" : "off" );
    printf( "Mipmaps %s\n", g_SaveOptions.bMipmaps ? "on" : "off" );
    if( g_bEnableGlobalIndex ) printf( "Global Index starting at %ld\n", g_nGlobalIndex ); else printf( "Global Index disabled\n" );
    printf( "Colour format: " );
    switch( g_SaveOptions.ColourFormat )
    {
        case ICF_SMART:     printf( "SMART\n" ); break;
        case ICF_4444:      printf( "4444\n" ); break;
        case ICF_1555:      printf( "1555\n" ); break;
        case ICF_565:       printf( "565\n" ); break;
        case ICF_555:       printf( "555\n" ); break;
        case ICF_SMARTYUV:  printf( "SMARTYUV\n" ); break;
        case ICF_YUV422:    printf( "YUV422\n" ); break;
        default: assert(false);
    }
    if( g_SaveOptions.nPaletteDepth ) printf( "Palette Depth: %dbpp\n", g_SaveOptions.nPaletteDepth );

    if( g_bVQCompress )
    {
        printf( "VQ Compression on\n" );

        switch( g_VQCompressor.m_Dither )
        {
            case VQNoDither:     printf( "VQ: no dither\n" ); break;
            case VQSubtleDither: printf( "VQ: half dither\n" ); break;
            case VQFullDither:   printf( "VQ: full dither\n" ); break;
        }
        switch( g_VQCompressor.m_Metric )
        {
            case VQMetricEqual:    printf( "VQ: no weighting\n" ); break;
            case VQMetricWeighted: printf( "VQ: eye-weighting\n" ); break;
        }
    }
    printf( "\n" );
}


//////////////////////////////////////////////////////////////////////
// Loads an alpha channel for the given image
//////////////////////////////////////////////////////////////////////
bool LoadAlpha( CImage& Image, const char* pszFilename )
{
    bool bChanged = false;

    //try to load alpha channel prefix file
    char szAlphaFilename[MAX_PATH] = "";
    if( *g_pszAlphaPrefix != '\0' )
    {
        //add the alpha prefix
        PrefixFileName( szAlphaFilename, pszFilename, g_pszAlphaPrefix );

        //load it
        printf( "Alpha: %s ...", szAlphaFilename );
        if( !Image.Load( szAlphaFilename, true ) ) *szAlphaFilename = '\0'; else bChanged = true;
    }

    //try to load alpha from default alpha file
    if( *szAlphaFilename == '\0' && *g_pszAlphaFilename != '\0' )
    {
        strcpy( szAlphaFilename, g_pszAlphaFilename );
        printf( "Alpha: %s ...", szAlphaFilename );
        bChanged = Image.Load( szAlphaFilename, true );
    }

    return bChanged;
}



//////////////////////////////////////////////////////////////////////
// Batch loads all mipmap levels for the given image
//////////////////////////////////////////////////////////////////////
bool BatchLoadMipmap( CImage& Image, const char* pszFilename )
{
    //get the MMRGBA object and calculate how many mipmaps it should have
    MMRGBA* pRGBA = Image.GetMMRGBA();
    if( pRGBA->bPalette ) return ReturnError( "Batch mipmapping can only be done on 32-bit images", pszFilename );
    if( pRGBA->pRGB == NULL ) return false;
    int nMipMaps = pRGBA->CalcMipMapsFromWidth();

    //set alpha flag
    bool bAlpha = (pRGBA->pAlpha != NULL);

    //allocate the full image
    MMRGBA mmrgba;
    mmrgba.Init( MMINIT_RGB|(bAlpha?MMINIT_ALPHA:0)|MMINIT_MIPMAP, Image.GetWidth(), Image.GetHeight() );

    //FIXME palettes!

    //copy over the first one
    mmrgba.pRGB[0] = pRGBA->pRGB[0]; pRGBA->pRGB[0] = NULL;
    if( bAlpha ) { mmrgba.pAlpha[0] = pRGBA->pAlpha[0];pRGBA->pAlpha[0] = NULL; }

    //load all mipmap levels
    printf( "Loading batch...\n" );
    for( int iMipMap = 1; iMipMap < nMipMaps; iMipMap++ )
    {
        //build filename
        char szMMFilename[MAX_PATH+1], szPrefix[11];
        snprintf( szPrefix, (sizeof (szPrefix)), "%d", iMipMap );
        PrefixFileName( szMMFilename, pszFilename, szPrefix );

        //load the image
        CImage MMImage;
        char szDimension[24]; snprintf( szDimension, (sizeof (szDimension)), "%dx%d", (pRGBA->nWidth >> iMipMap), (pRGBA->nHeight >> iMipMap) );
        printf( "%10s Image: %s ...", szDimension, szMMFilename );
        if( MMImage.Load( szMMFilename ) )
        {
            MMRGBA* pMMRGBA = MMImage.GetMMRGBA();
            if( pMMRGBA->bPalette ) return ReturnError( "Batch mipmapping can only be done on 32-bit images", szMMFilename );

            //make sure the loaded image is the right size!
            if( pMMRGBA->nWidth == (pRGBA->nWidth >> iMipMap) &&  pMMRGBA->nHeight == (pRGBA->nHeight >> iMipMap) )
            {
                //replace the current mipmap with the new, loaded one
                mmrgba.pRGB[iMipMap] = pMMRGBA->pRGB[0];
                pMMRGBA->pRGB[0] = NULL;

                //if they want an alpha...
                if( bAlpha )
                {
                    //load the alpha channel file
                    if( !LoadAlpha( MMImage, szMMFilename ) ) return ReturnError( "Batch failed: No alpha" );

                    //replace it
                    if( mmrgba.pAlpha && pMMRGBA->pAlpha && pMMRGBA->pAlpha[0] )
                    {
                        mmrgba.pAlpha[iMipMap] = pMMRGBA->pAlpha[0];
                        pMMRGBA->pAlpha[0] = NULL;
                    }
                }
            }

            printf( "Done.\n" );
        }
        else
        {
            //if the orignal image has no mipmap either, we can't continue
            //we *could* generate it automatically from the main image, but
            //that would defeat the purpose of batch-loading mipmaps.
            if( pRGBA->nMipMaps > 1 && pRGBA->pRGB[iMipMap] == NULL ) return ReturnError( "Batch failed: No mipmap" );
            else
            {
                printf( "Failed, using image's.\n" );
                mmrgba.pAlpha[iMipMap] = pRGBA->pRGB[iMipMap];
                pRGBA->pRGB[iMipMap] = NULL;
            }

        }
    }

    //replace it and return
    pRGBA->ReplaceWith( &mmrgba );
    printf( "\n" );
    return true;
}



//////////////////////////////////////////////////////////////////////
// Called by CCommandLineProcessor::ProcessAllFiles
//////////////////////////////////////////////////////////////////////
bool ProcessFile( const char* pszFilename )
{
    /* load image */

    //load image and alpha channel
    printf( "\nLoading: %s ...", pszFilename );
    CImage Image;
    if( Image.Load( pszFilename ) )
    {
        /* load alpha prefix file */
        LoadAlpha( Image, pszFilename );


        /* load/build all mipmap levels */
        if( g_bBatchMipmap ) if( !BatchLoadMipmap( Image, pszFilename ) )
        {
            g_nGlobalIndex++;
            g_nFailed++;
            return true;
        }
        if( g_bPagedMipmap )
        {
            //convert paged mipmaps to mipmaps
            DisplayStatusMessage( "Creating mipmaps from mipmap page..." );
            Image.PageToMipmaps();
        }

        /* apply before-processing image manipulation functions */

        //apply flips
        Image.Flip( g_bHFlip, g_bVFlip );

        //enlarge the image to a power of 2 if requested
        if( g_bEnlargeToPow2 ) { DisplayStatusMessage( "Enlarging to power of 2..." ); Image.EnlargeToPow2(); }

        //resize the image so it is square
        if( g_bMakeSquare ) { DisplayStatusMessage( "Making image square..." ); Image.MakeSquare(); }

        //shrink the image if requested
        if( g_bHalfSize ) { DisplayStatusMessage( "Shrinking..." ); Image.ScaleHalfSize(); }


        //display Ninja-friendly warning
        if( g_nGlobalIndex > MAX_GBIX ) printf( "\nWarning: Global index > 0x%X - this may cause problems if you're using Ninja\n", MAX_GBIX );

        //we can only have a .vqf file if we're doing vq compressing
        const char* pszPreferredExtension = g_bVQCompress ? g_pszOutputExtension : "PVR";


        //build save file name
        char szSaveFilename[MAX_PATH];
        strcpy( szSaveFilename, g_pszOutputPath );
        if( *g_pszOutputPath != '\0' )
        {
            //ensure there's a trailing \ on the file
            char cLastChar = szSaveFilename[strlen(szSaveFilename)-1];
            if( cLastChar != '\\' && cLastChar != '/' ) strcat( szSaveFilename, "\\" );
        }
        strcat( szSaveFilename, GetFileNameNoPath(pszFilename) );
        strcpy( (char*)GetFileExtension(szSaveFilename), pszPreferredExtension );


        //VQ compress the image if the user asked for it and we can
        if( g_bVQCompress && Image.CanVQ()  )
        {
            //generate the VQ image etc.
            printf( "VQ compressing..." );

            CVQImage* pVQImage = g_VQCompressor.GenerateVQ( &Image );

            if( pVQImage != NULL )
            {
                //export it
                pVQImage->ExportFile( szSaveFilename );
                delete pVQImage;
                g_nSucceeded++;
            }
            else
                g_nFailed++;
        }
        else
        {
            //display a message indicating that VQ compression won't be done on this image
            if( g_bVQCompress ) printf( "Can't VQ...doing non-VQ..." );

            //generate mipmaps if the image doesn't have any
            if( Image.GetNumMipMaps() <= 1 && g_SaveOptions.bMipmaps )
            {
                printf( "Building mipmaps..." );
                Image.GenerateMipMaps();
                printf( "done. " );
            }

            //export it
            printf( "Saving: %s ...", szSaveFilename );
            if( Image.Save( szSaveFilename, &g_SaveOptions ) )
            {
                g_nSucceeded++;
                printf( "done.\n" );
            }
            else
            {
                printf( "failed.\n" );
                g_nFailed++;
            }
        }
    }
    else
        g_nFailed++;

    //increment global index
    g_nGlobalIndex++;
    return true;
}


//////////////////////////////////////////////////////////////////////
// Program entry point
//////////////////////////////////////////////////////////////////////
int main( int argc, char* argv[] )
{
    clock_t start = clock();

    /* track memory leaks on exit */
#ifdef _DEBUG
    _CrtSetDbgFlag( _CRTDBG_LEAK_CHECK_DF );
#endif

    /* set defaults */
    g_SaveOptions.bMipmaps = false;
    g_SaveOptions.bTwiddled = false;
    g_SaveOptions.bPad = false;
    g_SaveOptions.ColourFormat = ICF_SMART;

    CCommandLineProcessor CommandLine( argc, argv );

    /* initialise the command line processor */
    bool bShowHelp = false, bShowExamples = false, bQuiet = false, bTimeTask = false, bShowParameters = false, bReverseAlpha = false;
    const char * pszColourFormat;
    pszColourFormat = "SMART";
    g_pszAlphaFilename = "";
    g_pszAlphaPrefix = "";
    g_pszOutputExtension = "PVR";
    g_pszOutputPath = "";

    int nVQDither = 0, nVQWeighting = 0;

    //add all command line switches to the command line processor
    CommandLine.RegisterCommandLineOption( "HELP",           "?",  0, "displays help",                                           CLF_NONE,    &bShowHelp );
    CommandLine.RegisterCommandLineOption( "EXAMPLE",        "EG", 0, "displays examples",                                       CLF_NONE,    &bShowExamples );
    CommandLine.RegisterCommandLineOption( "SHOWPARAMS",     "SP", 0, "displays an overview of the parameters selected",         CLF_NONE,    &bShowParameters );
    CommandLine.RegisterCommandLineOption( "QUIET",          "Q",  0, "does not display output (except errors)",                 CLF_NONE,    &bQuiet );
    CommandLine.RegisterCommandLineOption( "TIMETASK",       "TT", 0, "display the time taken to complete the task",             CLF_NONE,    &bTimeTask );
    CommandLine.AddGap();

    CommandLine.RegisterCommandLineOption( "OUTPATH",        "OP", 1, "[path] output path",                                      CLF_NONE,    &g_pszOutputPath );
    CommandLine.RegisterCommandLineOption( "OUTFILE",        "OF", 1, "[extension] output extension: PVR VQF C",                 CLF_SHOWDEF, &g_pszOutputExtension );
    CommandLine.AddGap();

    CommandLine.RegisterCommandLineOption( "TWIDDLE",        "TW", 0, "twiddle the surface",                                     CLF_NONE,    &g_SaveOptions.bTwiddled );
    CommandLine.RegisterCommandLineOption( "MIPMAP",         "MM", 0, "generate/save mipmaps",                                   CLF_NONE,    &g_SaveOptions.bMipmaps );
    CommandLine.RegisterCommandLineOption( "COLOURFORMAT",   "CF", 1, "[format] SMART 4444 1555 565 555 SMARTYUV YUV422 8888",   CLF_SHOWDEF, &pszColourFormat );
    CommandLine.RegisterCommandLineOption( "PALETTEDEPTH",   "PD", 1, "[n] 0 = no palette (default), 4 = 4bpp, 8 = 8bpp",        CLF_NONE,    &g_SaveOptions.nPaletteDepth );
    CommandLine.RegisterCommandLineOption( "GBIX",           "GI", 1, "[n] initial global index. Incremented for each file",     CLF_NONE,    &g_nGlobalIndex, &g_bEnableGlobalIndex );
    CommandLine.AddGap();

    CommandLine.RegisterCommandLineOption( "ALPHAPREFIX",    "AP", 1, "[prefix] load alpha from file with this prefix",          CLF_NONE,    &g_pszAlphaPrefix );
    CommandLine.RegisterCommandLineOption( "ALPHAFILE",      "AF", 1, "[file] load alpha channel from this file instead",        CLF_NONE,    &g_pszAlphaFilename );
    CommandLine.RegisterCommandLineOption( "INVERSEALPHA",   "IA", 0, "inverse alpha so 0xFF = transparent",                     CLF_NONE,    &bReverseAlpha );
    CommandLine.RegisterCommandLineOption( "PADEND",         "PE", 0, "pads the end of a stride texture: eg 640x480->1024x512",  CLF_NONE,    &g_SaveOptions.bPad );
    CommandLine.RegisterCommandLineOption( "RESIZEPOW2",     "P2", 0, "resizes the image so width & height are powers of 2",     CLF_NONE,    &g_bEnlargeToPow2 );
    CommandLine.RegisterCommandLineOption( "MAKESQUARE",     "MS", 0, "resizes the image so it is square",                       CLF_NONE,    &g_bMakeSquare );
    CommandLine.RegisterCommandLineOption( "VFLIP",          "VF", 0, "flips the image vertically before processing",            CLF_NONE,    &g_bVFlip );
    CommandLine.RegisterCommandLineOption( "HFLIP",          "HF", 0, "flips the image horizontally before processing",          CLF_NONE,    &g_bHFlip );
    CommandLine.RegisterCommandLineOption( "HALFSIZE",       "HS", 0, "resamples the image to half size before processing",      CLF_NONE,    &g_bHalfSize );
    CommandLine.AddGap();

    CommandLine.RegisterCommandLineOption( "PAGEMIPMAP",     "PM", 0, "loads mipmaps from a single 1:2 size image",              CLF_NONE,    &g_bPagedMipmap, &g_SaveOptions.bMipmaps );
    CommandLine.RegisterCommandLineOption( "BATCHMIPMAP",    "BM", 0, "batch-loads mipmaps. eg: F=actual 1F=1/2...nF=1x1",       CLF_NONE,    &g_bBatchMipmap, &g_SaveOptions.bMipmaps );
    CommandLine.AddGap();

    CommandLine.RegisterCommandLineOption( "VQCOMPRESS",     "VQ", 0, "enables VQ compression",                                  CLF_NONE,    &g_bVQCompress );
    CommandLine.RegisterCommandLineOption( "VQDITHER",       "VD", 1, "VQ dither option: 0 = none, 1 = half, 2 = full",          CLF_SHOWDEF, &nVQDither, &g_bVQCompress );
    CommandLine.RegisterCommandLineOption( "VQWEIGHTING",    "VW", 1, "VQ weighting option: 0 = none, 1 = eye-weighted",         CLF_SHOWDEF, &nVQWeighting, &g_bVQCompress );
    CommandLine.RegisterCommandLineOption( "VQHFREQTOL",     "VT", 0, "compressor tolerates more inacuracy in high freq. changes",CLF_NONE,    &g_VQCompressor.m_bTolerateHigherFrequency, &g_bVQCompress );


    /* parse the command line */
    if( argc <= 1 )
    {
        //if there's no command line parameters, display help only
        bShowHelp = true;
    }
    else
    {
        //tell the command line processor to process commands
        if( !CommandLine.ParseCommandLine() )
        {
            ShowErrorMessage( CommandLine.m_szErrorMessage );
            return -1;
        }
    }


    /* process command line options */

    //get the version number from the VQ dll
    //char szVQVersion[256];
    //VqGetVersionInfoString( szVQVersion, "FileVersion" );

    //process program parameters and display the banner
    bool bContinue = true;
    if( bQuiet ) fclose(stdout);

    printf( "\n%s [%s]\n%s\n\n", szApplication, szVersion, szOtherInfo );
    if( bShowHelp )
    {
        //display the command line options
        CommandLine.DisplayCommandLineOptions();

        //display all supported file formats
        printf("\nExtensions supported: " );
        for( int i = 0; g_pszSupportedFormats[i] != NULL; i+=2 )
        {
            //remove ; . and * from it and display it [cheap hack]
            const char* pszTmp = g_pszSupportedFormats[i];
            while( *pszTmp ) { if( *pszTmp != '*' && *pszTmp != '.' ) printf( "%c", (*pszTmp==';'?' ':*pszTmp) ); pszTmp++; }
            printf( " " );
        }
        printf( "\n" );

        //don't continue
        bContinue = false;
    }
    if( bShowExamples )
    {
        const char* pszApp = CommandLine.GetAppFilename();
        printf( "\n\nExamples:\n" );
        printf( "\n\t%s *.TGA -TWIDDLE -MIPMAP -GBIX 1000\n\tconvert all tga files in the current directory to twiddled,\n\tmipmapped pvr files with global index headers starting at 1000\n", pszApp );
        printf( "\n\t%s *.BMP -VQCOMPRESS -MIPMAP\n\tconvert all bmp files in the current directory to vq compressed\n\tmipmapped pvr files (automatically twiddled)\n", pszApp );
        printf( "\n\t%s foo.bmp -CF 4444 -AF ..\\alphas\\t \n\tconvert \"foo.bmp\" to \"foo.pvr\" with RGB-4444,\n\tusing \"..\\alphas\\tfoo.bmp\" for the alpha channel\n", pszApp );
        printf( "\n\t%s foo.bmp -BATCHMIPMAP\n\tBuilds mipmaps from foo.bmp, 1foo.bmp, 2foo.bmp ... Nfoo.bmp\n", pszApp );
        printf( "\n\t%s foo.tga -BATCHMIPMAP -ALPHAPREFIX a -CF 4444\n\tBuilds mipmaps with alpha from\n\tfoo.tga + afoo.tga, 1foo.tga + a1foo.tga ... Nfoo.tga + aNfoo.tga\n\twhere Nfoo.tga is the 1x1 image\n", pszApp );
        printf( "\n\t%s @options.lst\n\tuse command line options specified in the file \"options.lst\"\n", pszApp );
        bContinue = false;
    }

    //do the processing if we should continue
    if( bContinue )
    {
        if( bReverseAlpha ) g_nOpaqueAlpha = 0x00;

        //set other parameters
        if( stricmp( pszColourFormat, "SMART" ) == 0 )      { g_SaveOptions.ColourFormat = g_VQCompressor.m_icf = ICF_SMART;  } else
        if( stricmp( pszColourFormat, "4444" ) == 0 )       { g_SaveOptions.ColourFormat = g_VQCompressor.m_icf = ICF_4444;   } else
        if( stricmp( pszColourFormat, "1555" ) == 0 )       { g_SaveOptions.ColourFormat = g_VQCompressor.m_icf = ICF_1555;   } else
        if( stricmp( pszColourFormat, "565" ) == 0 )        { g_SaveOptions.ColourFormat = g_VQCompressor.m_icf = ICF_565;    } else
        if( stricmp( pszColourFormat, "555" ) == 0 )        { g_SaveOptions.ColourFormat = g_VQCompressor.m_icf = ICF_555;    } else
        if( stricmp( pszColourFormat, "SMARTYUV" ) == 0 )   { g_SaveOptions.ColourFormat = g_VQCompressor.m_icf = ICF_SMARTYUV;  } else
        if( stricmp( pszColourFormat, "YUV422" ) == 0 )     { g_SaveOptions.ColourFormat = g_VQCompressor.m_icf = ICF_YUV422; } else
        if( stricmp( pszColourFormat, "8888" ) == 0 )
        {
            if( g_SaveOptions.nPaletteDepth == 0 ) { DisplayStatusMessage( "8888 specified with no palette depth - assuming a depth of 8bpp" ); g_SaveOptions.nPaletteDepth = 8; }
            g_SaveOptions.ColourFormat = g_VQCompressor.m_icf = ICF_8888;
        }
        else
        { ShowErrorMessage( "%s - unknown colour format", pszColourFormat ); return -1; }
        switch( nVQDither )
        {
            case 0: g_VQCompressor.m_Dither = VQNoDither; break;
            case 1: g_VQCompressor.m_Dither = VQSubtleDither; break;
            case 2: g_VQCompressor.m_Dither = VQFullDither; break;
            default: ShowErrorMessage( "%d - unknown dither option", nVQDither ); return -1;
        }
        switch( nVQWeighting )
        {
            case 0: g_VQCompressor.m_Metric = VQMetricEqual; break;
            case 1: g_VQCompressor.m_Metric = VQMetricWeighted; break;
            default: ShowErrorMessage( "%d - unknown weighting option", nVQWeighting ); return -1;
        }
        if( g_SaveOptions.nPaletteDepth )
        {
            if( g_SaveOptions.nPaletteDepth != 4 && g_SaveOptions.nPaletteDepth != 8 )
            {
                ShowErrorMessage( "%d - unknown palette depth", g_SaveOptions.nPaletteDepth );
                return -1;
            }

            DisplayStatusMessage( "Palettised VQ is not supported. Disabling VQ compression.\n" );
            g_bVQCompress = false;
        }
        if( g_bPagedMipmap && g_bBatchMipmap )
        {
            ShowErrorMessage( "Paged and batch mipmap loading options cannot be used together\n" );
            return -1;

        }
        g_VQCompressor.m_bMipmap = g_SaveOptions.bMipmaps;

        /* display the parameters before we start processing files */
        if( bShowParameters ) DisplayParameters();

        /* build the twiddle table */
        BuildTwiddleTable();

        /* process all pictures */
        if( CommandLine.ProcessAllFiles( ProcessFile ) == false )
        {
            ShowErrorMessage( CommandLine.m_szErrorMessage );
            return -1;
        }


        /* display the length the task took to complete, if requested */
        if( bQuiet )
        {
            if( g_nFailed ) ShowErrorMessage( "\n%d files failed", g_nFailed );
        }
        else
        {
            DisplayStatusMessage( "\nAll done: %d files failed. %d files created OK. ", g_nFailed, g_nSucceeded );
            if( bTimeTask ) DisplayStatusMessage( "Total time: %.2f seconds", (double)(clock() - start) / CLOCKS_PER_SEC );
        }
    }

#ifdef _DEBUG
    printf( "Press any key to continue...\n" );
    getch();
#endif
}
