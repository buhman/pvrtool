/*************************************************
 Command Line Processing Helper Class
 
   This class provides useful processing functions
   for the extraction of multiple-argument command
   line parameters as well as providing a facility
   for processing files specified on the command
   line.

  To Do
  
    * More testing...

**************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "max_path.h"
#include "stricmp.h"
#include "findfirst.h"
#include "Util.h"
#include "CommandLineProcessor.h"

#ifdef _DEBUG
    #include <assert.h>
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CCommandLineProcessor::CCommandLineProcessor( int argc, char** argv )
{
    m_pCommandLineOptions = NULL;
    m_pCommandLineOptionListEnd = NULL;
    m_pFileSpecs = NULL;
    m_szErrorMessage[0] = '\0';
    m_argc = argc;
    m_argv = argv;
    m_pStringList = NULL;
}

CCommandLineProcessor::~CCommandLineProcessor()
{
    //delete all command line options
    while( m_pCommandLineOptions )
    {
        //get a pointer to the current item and advance the list
        CommandLineOptionList* temp = m_pCommandLineOptions;
        m_pCommandLineOptions = m_pCommandLineOptions->next;

        //delete this option
        delete temp;
    }

    //delete all filespecs
    while( m_pFileSpecs )
    {
        //get a pointer to the current item and advance the list
        StringList* temp = m_pFileSpecs;
        m_pFileSpecs = m_pFileSpecs->next;

        //delete this item
        delete[] temp->pszString;
        delete temp;
    }

    //delete all strings
    while( m_pStringList )
    {
        StringList* temp = m_pStringList;
        m_pStringList = m_pStringList->next;

        delete[] temp->pszString;
        delete temp;
    }
}



//////////////////////////////////////////////////////////////////////
// Command line type identifier
//////////////////////////////////////////////////////////////////////
CCommandLineProcessor::CommandLineType CCommandLineProcessor::GetCommandLineType( const char* pszCommandLine )
{
    if( pszCommandLine == NULL ) return CommandLine_Error;

    switch( *pszCommandLine )
    {
        case '/': //drop through (remove this for non-pc environments)
        case '-':  return CommandLine_Switch;
        case '@':  return CommandLine_Response;
        case '\0': return CommandLine_Error;
        default:   return CommandLine_String;
    }
}


//////////////////////////////////////////////////////////////////////
// Command line parameter registration functions
//////////////////////////////////////////////////////////////////////
bool CCommandLineProcessor::RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, bool* pbValue, bool* pbFound /*NULL*/ )
{
    //build command line option structure
    CommandLineOption Option;
    Option.Init( pszLongSwitch, pszShortSwitch, nCommandLineOptions, pszDescription, nFlags, pbFound );
    Option.pbValue = pbValue;

#ifdef _DEBUG
    assert( IsValid( &Option ) );
#endif

    //add it to the list
    return AddCommandLineOption( Option );
}


bool CCommandLineProcessor::RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, int* pnValue, bool* pbFound /*NULL*/ )
{
    //build command line option structure
    CommandLineOption Option;
    Option.Init( pszLongSwitch, pszShortSwitch, nCommandLineOptions, pszDescription, nFlags, pbFound );
    Option.pnValue = pnValue;

#ifdef _DEBUG
    //we can only have 0 parameters if this is a bool (i.e. /YESPLEASEDOTHIS )
    assert( nCommandLineOptions > 0 );
    assert( IsValid( &Option ) );
#endif

    //add it to the list
    return AddCommandLineOption( Option );
}


bool CCommandLineProcessor::RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, unsigned long int* puValue, bool* pbFound /*NULL*/ )
{
    //build command line option structure
    CommandLineOption Option;
    Option.Init( pszLongSwitch, pszShortSwitch, nCommandLineOptions, pszDescription, nFlags, pbFound );
    Option.puValue = puValue;

#ifdef _DEBUG
    //we can only have 0 parameters if this is a bool (i.e. /YESPLEASEDOTHIS )
    assert( nCommandLineOptions > 0 );
    assert( IsValid( &Option ) );
#endif

    //add it to the list
    return AddCommandLineOption( Option );
}


bool CCommandLineProcessor::RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, float* pfValue, bool* pbFound /*NULL*/ )
{
    //build command line option structure
    CommandLineOption Option;
    Option.Init( pszLongSwitch, pszShortSwitch, nCommandLineOptions, pszDescription, nFlags, pbFound );
    Option.pfValue = pfValue;

#ifdef _DEBUG
    //we can only have 0 parameters if this is a bool (i.e. /YESPLEASEDOTHIS )
    assert( nCommandLineOptions > 0 );
    assert( IsValid( &Option ) );
#endif

    //add it to the list
    return AddCommandLineOption( Option );
}


bool CCommandLineProcessor::RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, char** ppszValue, bool* pbFound /*NULL*/ )
{
    //build command line option structure
    CommandLineOption Option;
    Option.Init( pszLongSwitch, pszShortSwitch, nCommandLineOptions, pszDescription, nFlags, pbFound );
    Option.ppszValue = ppszValue;

#ifdef _DEBUG
    //we can only have 0 parameters if this is a bool (i.e. /YESPLEASEDOTHIS )
    assert( nCommandLineOptions > 0 );
    assert( IsValid( &Option ) );
#endif

    //add it to the list
    return AddCommandLineOption( Option );
}



//////////////////////////////////////////////////////////////////////
// Adds an empty command line. Causes a gap to be displayed when parameters are listed
//////////////////////////////////////////////////////////////////////
void CCommandLineProcessor::AddGap()
{
    //add a blank option
    CommandLineOption Option;
    memset( &Option, 0, sizeof(Option) );
    AddCommandLineOption( Option );
}



//////////////////////////////////////////////////////////////////////
// Adds the given command line option to the options list
//////////////////////////////////////////////////////////////////////
bool CCommandLineProcessor::AddCommandLineOption( const CommandLineOption& Option )
{
    //create the new item
    CommandLineOptionList* pNew = new CommandLineOptionList;
    pNew->Option = Option;
    pNew->next = NULL;

    //add it to the end of the list
    if( m_pCommandLineOptionListEnd == NULL )
    {
        m_pCommandLineOptionListEnd = m_pCommandLineOptions = pNew;
    }
    else
    {
        m_pCommandLineOptionListEnd->next = pNew;
        m_pCommandLineOptionListEnd = pNew;
    }

    return true;
}


//////////////////////////////////////////////////////////////////////
// Displays the program options and parameters with default values etc.
//////////////////////////////////////////////////////////////////////
void CCommandLineProcessor::DisplayCommandLineOptions()
{
    //display usage banner
    printf( "Usage: %s [@responsefile] [options] [files]\n\nOptions:\n", GetAppFilename() );

    //make sure there's something to display (note: there's not much point in using this class without any options!!)
    if( m_pCommandLineOptions == NULL )
    {
        printf( "no options!\n" );
        return;
    }

    //calculate the maximum string length of the long command line switches
    int nMaxLongSwitchWidth = 1;
    CommandLineOptionList* pList = m_pCommandLineOptions;
    while( pList )
    {
        if( pList->Option.pszLongSwitch )
        {
            int nLength = strlen( pList->Option.pszLongSwitch );
            if( nLength > nMaxLongSwitchWidth ) nMaxLongSwitchWidth = nLength;
        }

        pList = pList->next;
    }

    //display all command line switches
    pList = m_pCommandLineOptions;
    while( pList )
    {
        if( pList->Option.pszLongSwitch )
        {
            //display option
            printf( "-%-2s -%-*s %s", pList->Option.pszShortSwitch, nMaxLongSwitchWidth, pList->Option.pszLongSwitch, pList->Option.pszDescription );

            //display default parameter value if requested
            if( pList->Option.nFlags & CLF_SHOWDEF && pList->Option.nCommandLineOptions )
            {
                printf( " [def:" );
                for( int i = 0; i < pList->Option.nCommandLineOptions; i++ )
                {           
                    if( pList->Option.pbValue )   printf( " %s", pList->Option.pbValue[i] ? "YES" : "NO" );
                    if( pList->Option.pnValue )   printf( " %d", pList->Option.pnValue[i] );
                    if( pList->Option.ppszValue ) printf( " %s", pList->Option.ppszValue[i] );
                    if( pList->Option.pfValue )   printf( " %.03f", pList->Option.pfValue[i] );
                }
                printf( "]");
            }

        }

        //start new line and advance list
        printf( "\n" );
        pList = pList->next;
    }
}



//////////////////////////////////////////////////////////////////////
// Locates the given command line option in the list
//////////////////////////////////////////////////////////////////////
CCommandLineProcessor::CommandLineOption* CCommandLineProcessor::FindCommandLineOption( const char* pszSwitch )
{
    //find it in the list
    CommandLineOptionList* pList = m_pCommandLineOptions;
    while( pList )
    {
        //compare the switch to the long and short switches
        if( ( pList->Option.pszShortSwitch && stricmp(pList->Option.pszShortSwitch, pszSwitch) == 0 ) || ( pList->Option.pszLongSwitch && stricmp(pList->Option.pszLongSwitch, pszSwitch) == 0 ) )
            return &pList->Option;

        //advance the list
        pList = pList->next;
    }

    return NULL;
}


//////////////////////////////////////////////////////////////////////
// Parses the command line and sets all options
//////////////////////////////////////////////////////////////////////
bool CCommandLineProcessor::ParseCommandLine()
{
    int argi;

    //process all command line options
    for( argi = 1; argi < m_argc; argi++ )
    {
        switch( GetCommandLineType( m_argv[argi] ) )
        {
            case CommandLine_String:
            {
                //it's a non-switch parameter - add it to the non-switch list
                StringList* pNew = new StringList;
                pNew->next = m_pFileSpecs;
                pNew->pszString = new char[ strlen(m_argv[argi]) + 1 ];
                strcpy( pNew->pszString, m_argv[argi] );
                m_pFileSpecs = pNew;
                break;
            }

            case CommandLine_Switch:
            {
                //it's a switch - find the switch options
                CommandLineOption* pOption = FindCommandLineOption( &m_argv[argi][1] );
                if( pOption )
                {
                    //set it's "option found" flag, if it has one
                    if( pOption->pbFound ) *pOption->pbFound = true;

                    //it matches - set the options
                    if( pOption->nCommandLineOptions == 0 )
                    {
                        //set this switches value to true
                        if( pOption->pbValue ) *pOption->pbValue = true;
                    }
                    else
                    {
                        //make sure we've got enough command line parameters left
                        if( argi+pOption->nCommandLineOptions < m_argc )
                        {
                            //store all parameters
                            for( int i = 0; i < pOption->nCommandLineOptions; i++ )
                                pOption->SetOption( i, m_argv[++argi] );
                        }
                        else
                        {
                            sprintf( m_szErrorMessage, "%s - too few parameters", m_argv[argi] );
                            return false;
                        }
                    }
                }
                else
                {
                    //we didn't find it - set the error message and return
                    sprintf( m_szErrorMessage, "%s - unknown option", m_argv[argi] );
                    return false;
                }
                break;
            }

            case CommandLine_Response:
                if( !ProcessResponseFile( &m_argv[argi][1] ) ) return false;
                break;
            case CommandLine_Error:
                sprintf( m_szErrorMessage, "null character %d", argi );
                return false;
                break;
        }
    }

    return true;
}


//////////////////////////////////////////////////////////////////////
// Processes each command line option in the given response file
//////////////////////////////////////////////////////////////////////
bool CCommandLineProcessor::ProcessResponseFile(const char *pszFilename)
{
    //open the file
    FILE* file = fopen( pszFilename, "rt" );
    if( file == NULL )
    {
        sprintf( m_szErrorMessage, "%s - can't open file", pszFilename );
        return false;
    }

    //load the file into a buffer
    fseek( file, 0, SEEK_END ); int nFileLength = ftell( file ); fseek( file, 0, SEEK_SET );
    if( nFileLength == -1 )
    {
        sprintf( m_szErrorMessage, "%s - invalid file", pszFilename );
        fclose(file);
        return false;
    }
    char* pBuffer = (char*)malloc( nFileLength );
    memset( pBuffer, 0, nFileLength );
    fread( pBuffer, 1, nFileLength, file );
    fclose(file);

    //read & process each line
    char* pPtr = pBuffer;
    while( *pPtr )
    {
        //skip whitespace
        while( *pPtr != '\0' && (*pPtr == ' ' || *pPtr == '\t' || *pPtr == '\r' || *pPtr == '\n' ) ) pPtr++;
        if( *pPtr == '\0' ) break;

        //get a pointer to the first word
        char* pszWord = pPtr;
        while( *pPtr != '\0' && *pPtr != ' ' && *pPtr != '\t' && *pPtr != '\r' && *pPtr != '\n' ) pPtr++;
        bool bEndOfBuffer = (*pPtr == '\0');
        *pPtr = '\0';

        //advance the pointer
        if( !bEndOfBuffer ) pPtr++;

        //find this command line switch
        switch( GetCommandLineType( pszWord ) )
        {
            case CommandLine_String:
            {
                //it's a non-switch parameter - add it to the non-switch list
                StringList* pNew = new StringList;
                pNew->next = m_pFileSpecs;
                pNew->pszString = new char[ strlen(pszWord) + 1 ];
                strcpy( pNew->pszString, pszWord );
                m_pFileSpecs = pNew;
                break;
            }

            case CommandLine_Switch:
            {
                //it's a switch - find the switch options
                CommandLineOption* pOption = FindCommandLineOption( &pszWord[1] );
                if( pOption )
                {
                    //set it's "option found" flag, if it has one
                    if( pOption->pbFound ) *pOption->pbFound = true;

                    //it matches - set the options
                    if( pOption->nCommandLineOptions == 0 )
                    {
                        //set this switches value to true
                        if( pOption->pbValue ) *pOption->pbValue = true;
                    }
                    else
                    {
                        //store all parameters
                        for( int i = 0; i < pOption->nCommandLineOptions; i++ )
                        {

                            //skip whitespace
                            if( !bEndOfBuffer ) while( *pPtr != '\0' && (*pPtr == ' ' || *pPtr == '\t' || *pPtr == '\r' || *pPtr == '\n' ) ) pPtr++;

                            //fail if we're at the end of the line and there should be more parameters
                            if( *pPtr == '\0' || bEndOfBuffer ) 
                            {
                                sprintf( m_szErrorMessage, "@%s: %s - too few parameters", pszFilename, pszWord );
                                free( pBuffer ); 
                                return false;
                            }

                            //get next option for this command line
                            char* pszOption = pPtr;
                            while( *pPtr != '\0' && *pPtr != ' ' && *pPtr != '\t' && *pPtr != '\r' && *pPtr != '\n'  ) pPtr++;
                            bEndOfBuffer = (*pPtr == '\0');
                            *pPtr = '\0';

                            //if the value will be used as a string, create a duplicate
                            //(note: this is needed because unlike the argv[] strings, this
                            //       array is deleted when we leave the function)
                            if( pOption->ppszValue[i] )
                            {
                                //add a new item to the string list
                                StringList* pNew = new StringList;
                                pNew->next = m_pStringList;
                                pNew->pszString = new char[ strlen( pszOption ) + 1 ];
                                strcpy( pNew->pszString, pszOption );
                                m_pStringList = pNew;

                                //change the option pointer to the duplicated string
                                pszOption = pNew->pszString;
                            }
                            
                            //set the option
                            pOption->SetOption( i, pszOption );

                            //skip over terminator/whitespace string terminator
                            if( !bEndOfBuffer ) pPtr++;
                        }
                    }
                }
                else
                {
                    //we didn't find it - set the error message and return
                    sprintf( m_szErrorMessage, "@%s: %s - unknown option", pszFilename, pszWord  );
                    free( pBuffer ); 
                    return false;
                }
                break;
            }

            case CommandLine_Response:
                sprintf( m_szErrorMessage, "@%s: %s - can't have response file in response file", pszFilename, pszWord );
                free( pBuffer ); 
                return false;
            case CommandLine_Error:
                sprintf( m_szErrorMessage, "null character");
                return false;
        }
    }


    //clean up and return
    free( pBuffer );
    return true;
}



//////////////////////////////////////////////////////////////////////
// Passes each file (or matching files) specified on the command line to the given function
//////////////////////////////////////////////////////////////////////
bool CCommandLineProcessor::ProcessAllFiles( FILEPROCESSINGFUNC pfnProcessFile )
{
    //validate parameters
    if( pfnProcessFile == NULL ) return false;

    //make sure there's some files
    if( m_pFileSpecs == 0 )
    {
        strcpy( m_szErrorMessage, "No files specified" );
        return false;
    }

    //process all filespecs
    int nFilesProcessed = 0;
    for( StringList* pFileSpec = m_pFileSpecs; pFileSpec != NULL; pFileSpec = pFileSpec->next )
    {
        //build the current path
        char szPath[MAX_PATH+1]; 
        strcpy( szPath, pFileSpec->pszString );
        char* pszPathEnd = (char*)GetFileNameNoPath( szPath );
        if( pszPathEnd == NULL ) pszPathEnd = szPath;
        *pszPathEnd = '\0';

        //find files matching the current filespec
        _finddata_t finddata;
        long hFind = _findfirst( pFileSpec->pszString, &finddata );
        if( hFind != -1 )
        {
            //process all matching files
            do 
            {
                //build this filename in full
                char szFilename[MAX_PATH+1];
                strcpy( szFilename, szPath );
                strcat( szFilename, finddata.name );

                //process this file
                if( pfnProcessFile( szFilename ) == false )
                {
                    //the callback returned false - stop processing
                    _findclose( hFind );
                    return false;
                }

                //increment file processed counter
                nFilesProcessed++;

            } while( _findnext( hFind, &finddata ) != -1 );
        }
        else
        {
            //indicate that we didn't find anything useful this time round, but continue anyway
            fprintf( stderr, "%s - no matching files found\n", pFileSpec->pszString );
        }

        _findclose( hFind );
    }

    //see if we managed to process anything
    if( nFilesProcessed == 0 )
    {
        strcpy( m_szErrorMessage, "No matching files were found" );
        return false;
    }

    return true;
}


//////////////////////////////////////////////////////////////////////
// Gets a pointer to the application's filename, without the path
//////////////////////////////////////////////////////////////////////
const char* CCommandLineProcessor::GetAppFilename()
{
    const char* pszThisAppFilename = &m_argv[0][ strlen( m_argv[0] ) ];
    while( *pszThisAppFilename != '/' && *pszThisAppFilename != '\\' && pszThisAppFilename != m_argv[0] ) pszThisAppFilename--;
    if( *pszThisAppFilename == '/' || *pszThisAppFilename == '\\' ) pszThisAppFilename++;
    return pszThisAppFilename;
}





//////////////////////////////////////////////////////////////////////
// Structure initialisation
//////////////////////////////////////////////////////////////////////
void CCommandLineProcessor::CommandLineOption::Init( const char* _pszLongSwitch, const char* _pszShortSwitch, int _nCommandLineOptions, const char* _pszDescription, int _nFlags, bool* _pbFound )
{
    pszLongSwitch = _pszLongSwitch;
    pszShortSwitch = _pszShortSwitch;
    nCommandLineOptions = _nCommandLineOptions;
    pszDescription = _pszDescription;
    nFlags = _nFlags;
    pbFound = _pbFound;

    pbValue = NULL;
    ppszValue = NULL;
    pnValue = NULL;
    puValue = NULL;
    pfValue = NULL;
}

//////////////////////////////////////////////////////////////////////
// Sets the given option
//////////////////////////////////////////////////////////////////////
void CCommandLineProcessor::CommandLineOption::SetOption( int i, char* pszOption )
{
    if( i >= nCommandLineOptions ) return;
    if( pbValue )
    {
        if( pszOption == NULL || *pszOption == '\0' ) pbValue[i] = false;
        else pbValue[i] = ( stricmp( pszOption, "YES" ) == 0 || stricmp( pszOption, "Y" ) == 0 || stricmp( pszOption, "TRUE" ) == 0 || stricmp( pszOption, "T" ) == 0 || stricmp( pszOption, "ON" ) == 0 || stricmp( pszOption, "1" ) == 0 );
    }
    if( pnValue )   pnValue[i] =   atoi( pszOption );
    if( puValue )   puValue[i] =   atol( pszOption );
    if( ppszValue )
    {
        ppszValue[i] = new char[ strlen(pszOption) + 1];
        strcpy( ppszValue[i], pszOption );
    }
    if( pfValue )   pfValue[i] =   (float)atof( pszOption );
}


//////////////////////////////////////////////////////////////////////
// Checks the command line configuration for errors (debug only)
//////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
bool CCommandLineProcessor::IsValid( CommandLineOption* pNewOption )
{
    //make sure it's got a long and a short switch
    if( pNewOption->pszLongSwitch == NULL || pNewOption->pszShortSwitch == NULL ) return false;

    //make sure the data pointers point somewhere
    if( pNewOption->pbValue == NULL && pNewOption->pfValue == NULL && pNewOption->pnValue == NULL && pNewOption->ppszValue == NULL && pNewOption->pbFound == NULL ) return false;

    //see if the option already exists
    CommandLineOptionList* pList = m_pCommandLineOptions;
    while( pList )
    {
        if( pList->Option.pszLongSwitch && pList->Option.pszShortSwitch )
        {
            //check long parameter
            if( stricmp( pList->Option.pszLongSwitch, pNewOption->pszLongSwitch ) == 0 || stricmp( pList->Option.pszLongSwitch, pNewOption->pszShortSwitch ) == 0 ) return false;

            //check short parameter
            if( stricmp( pList->Option.pszShortSwitch,  pNewOption->pszShortSwitch  ) == 0 || stricmp( pList->Option.pszShortSwitch,  pNewOption->pszLongSwitch ) == 0 ) return false;
        }

        //check next parameter
        pList = pList->next;
    }

    return true;
}
#endif
