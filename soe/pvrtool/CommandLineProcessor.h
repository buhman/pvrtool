// CommandLineProcessor.h: interface for the CCommandLineProcessor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COMMANDLINEPROCESSOR_H__530DE484_A97A_11D3_86DB_005004314EE7__INCLUDED_)
#define AFX_COMMANDLINEPROCESSOR_H__530DE484_A97A_11D3_86DB_005004314EE7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


//file processing callback function type
typedef bool (*FILEPROCESSINGFUNC)(const char* pszFilename);

//flags for RegisterCommandLineOption
#define CLF_NONE    (0)
#define CLF_SHOWDEF (1)

class CCommandLineProcessor
{
public:
	CCommandLineProcessor( int argc, char** argv );
	virtual ~CCommandLineProcessor();

    bool RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, bool* pbValue,              bool* pbFound = NULL );
    bool RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, int* pnValue,               bool* pbFound = NULL  );
    bool RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, unsigned long int* puValue, bool* pbFound = NULL  );
    bool RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, float* pfValue,             bool* pbFound = NULL  );
    bool RegisterCommandLineOption( const char* pszLongSwitch, const char* pszShortSwitch, int nCommandLineOptions, const char* pszDescription, int nFlags, const char ** ppszValue,           bool* pbFound = NULL  );
    void AddGap();

    bool ParseCommandLine();
    void DisplayCommandLineOptions();

    char m_szErrorMessage[256];

    bool ProcessAllFiles( FILEPROCESSINGFUNC pfnProcessFile );

    const char* GetAppFilename();

protected:
	char* CopyString( const char* pszString );
	bool ProcessResponseFile( const char* pszFilename );

    //command line option structure
    struct CommandLineOption
    {
        const char* pszLongSwitch;
        const char* pszShortSwitch;
        int nCommandLineOptions;
        const char* pszDescription;
        int nFlags;
        bool* pbFound;

        bool* pbValue;
        const char ** ppszValue;
        int* pnValue;
        unsigned long int* puValue;
        float* pfValue;

        void Init( const char* _pszLongSwitch, const char* _pszShortSwitch, int _nCommandLineOptions, const char* _pszDescription, int _nFlags, bool* _pbFound );
        void SetOption( int i, char* pszOption );
    };

    //list of command line options
    struct CommandLineOptionList
    {
        CommandLineOption Option;
        CommandLineOptionList* next;
    };

    //command line option list functions
    bool AddCommandLineOption( const CommandLineOption& Option );
    CommandLineOption* FindCommandLineOption( const char* pszSwitch );

    //command line option list pointers
    CommandLineOptionList* m_pCommandLineOptions;
    CommandLineOptionList* m_pCommandLineOptionListEnd;

#ifdef _DEBUG
	bool IsValid( CommandLineOption* pNewOption );
#endif



    //list of file specs
    struct StringList
    {
        char* pszString;
        StringList* next;
    };

    //file spec list
    StringList* m_pFileSpecs;
    StringList* m_pStringList;

    //internal argc & argv
    int m_argc;
    char** m_argv;

    //command line string type identification
    enum CommandLineType { CommandLine_Error, CommandLine_String, CommandLine_Switch, CommandLine_Response };
    CommandLineType GetCommandLineType( const char* pszCommandLine );
};

#endif // !defined(AFX_COMMANDLINEPROCESSOR_H__530DE484_A97A_11D3_86DB_005004314EE7__INCLUDED_)
