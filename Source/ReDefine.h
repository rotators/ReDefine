#pragma once

#include <functional>
#include <map>
#include <regex>
#include <string>
#include <utility>
#include <vector>

class Ini;

class ReDefine
{
public:

    struct ScriptCode;
    struct ScriptEditAction;

    //
    // misc maps
    //

    typedef std::map<std::string, std::map<int, std::string>>          DefinesMap;
    typedef std::map<std::string, std::vector<std::string>>            StringVectorMap;
    typedef std::map<std::string, std::map<std::string, unsigned int>> CountersMap;

    //
    // script edit actions
    //

    enum class ScriptEditReturn : signed char
    {
        Invalid = -1,
        Failure,
        Success
    };

    typedef std::function<ScriptEditReturn( ScriptEditAction&, const ScriptCode& )> ScriptEditIf;
    typedef std::function<ScriptEditReturn( ScriptEditAction&, ScriptCode& )>       ScriptEditDo;

    //
    // ReDefine
    //

    Ini*        Config;
    bool        Dev;

    std::string LogFile;
    std::string LogWarning;
    std::string LogDebug;

    struct SStatus
    {
        struct SCurrent
        {
            std::string  File;
            std::string  Line;
            unsigned int LineNumber;

            SCurrent();

            void         Clear();
        }
        Current;

        struct SProcess
        {
            // total number of files/lines processed

            unsigned int Files;
            unsigned int Lines;

            // total number of files/lines changes or (if running in read-only mode) change candidates

            unsigned int FilesChanges;
            unsigned int LinesChanges;

            CountersMap  Counters;  // <name, <value, count>>

            SProcess();

            void         Clear();
        }
        Process;

        void Clear();
    }
    Status;


    ReDefine();
    virtual ~ReDefine();

    void Init();
    void Finish();
    void RemoveLogs();

    bool ReadFile( const std::string& filename, std::vector<std::string>& lines );
    bool ReadFile( const std::string& filename, std::vector<char>& data );
    bool ReadConfig( const std::string& defines, const std::string& variable_prefix, const std::string& function_prefix, const std::string& raw, const std::string& script );

    void ProcessHeaders( const std::string& path );
    void ProcessScripts( const std::string& path, const bool readOnly = false );

    //
    // Defines
    //

    // script header definition
    struct Header
    {
        const std::string Filename;
        const std::string Type;
        const std::string Prefix;
        const std::string Suffix;
        const std::string Group;

        Header( const std::string& filename, const std::string& type, const std::string& prefix, const std::string& suffix, const std::string& group );
    };

    // holds [Defines] between reading configuration and processing headers steps
    std::vector<Header> Headers;

    DefinesMap          RegularDefines; // <type, <value, name>>
    DefinesMap          ProgramDefines; // <type, <value, names>>
    StringVectorMap     VirtualDefines; // <virtual_type, <types>>

    void FinishDefines();

    bool ReadConfigDefines( const std::string& sectionPrefix );

    bool IsDefineType( const std::string& type );
    bool IsRegularDefineType( const std::string& type );
    bool IsMysteryDefineType( const std::string& type );
    bool GetDefineName( const std::string& type, const int value, std::string& result, const bool skipVirtual = false );
    bool GetDefineValue( const std::string& type, const std::string& value, int& result, const bool skipVirtual = false );

    bool ProcessHeader( const std::string& path, const Header& header );
    bool ProcessValue( const std::string& type, std::string& value, const bool silent = false );
    void ProcessValueGuessing( std::string& value );

    //
    // Functions
    //

    struct FunctionProto
    {
        std::string              ReturnType;
        std::vector<std::string> ArgumentsTypes;
    };

    std::map<std::string, FunctionProto> FunctionsPrototypes;

    void FinishFunctions();

    bool ReadConfigFunctions( const std::string& section );

    void ProcessFunctionArguments( ScriptCode& function );

    //
    // Log
    //

    void DEBUG( const char* caller, const char* format, ... );
    void WARNING( const char* caller, const char* format, ... );
    void ILOG( const char* format, ... );
    void LOG( const char* format, ... );
    void SHOW( const char* format, ... );

    //
    // Operators
    //

    std::map<std::string, std::string> Operators; // <operatorName, operator>

    void InitOperators();
    void FinishOperators();

    bool        IsOperator( const std::string& op );
    bool        IsOperatorName( const std::string& opName );
    std::string GetOperator( const std::string& opName );
    std::string GetOperatorName( const std::string& op );

    void ProcessOperator( const std::string& type, ScriptCode& code );

    //
    // Raw
    //

    std::map<std::string, std::string> Raw;

    void FinishRaw();

    bool ReadConfigRaw( const std::string& section );

    void ProcessRaw( std::string& line );

    //
    // Script
    //

    struct ScriptEdit
    {
        struct Action
        {
            std::string              Name;
            std::vector<std::string> Values;
            bool                     Negate; // used by conditions only
        };

        bool                Debug;
        std::string         Name;

        std::vector<Action> Conditions;
        std::vector<Action> Results;

        ScriptEdit();
    };

    // Read-only version of ScriptEdit::Action (with extra helper functions),
    // passed to conditions/results functions
    struct ScriptEditAction
    {
        enum class Flag : unsigned char
        {
            BEFORE  = 0x01, // set when processing RunBefore edits
            AFTER   = 0x02, // set when processing RunAfter edits
            ______  = 0x04,

            RESTART = 0x10  // set by DoRestart; forces restart of line processing keeping changes already made to code
        };

        const std::string&              Name;
        const std::vector<std::string>& Values;

        ReDefine*                       Root;
        Flag&                           Flags;

        ScriptEditAction( void*  root, const ScriptEdit::Action& action, ScriptEditAction::Flag& flags  );

        //

        bool IsFlag( ScriptEditAction::Flag flag ) const;
        void SetFlag( ScriptEditAction::Flag flag );
        void UnsetFlag( ScriptEditAction::Flag flag );

        //

        inline ScriptEditReturn Return( bool result )
        {
            return result ? ScriptEditReturn::Success : ScriptEditReturn::Failure;
        }

        inline ScriptEditReturn Invalid()
        {
            return ScriptEditReturn::Invalid;
        }

        inline ScriptEditReturn Failure()
        {
            return ScriptEditReturn::Failure;
        }

        inline ScriptEditReturn Success()
        {
            return ScriptEditReturn::Success;
        }

        //

        bool IsBefore( const char* caller ) const;
        bool IsAfter( const char* caller ) const;
        bool IsValues( const char* caller, const unsigned int& count ) const;

        bool GetINDEX( const char* caller, const unsigned int& val, const ReDefine::ScriptCode& code, unsigned int& out ) const;
        bool GetTYPE( const char* caller, const unsigned int& val, bool allowUnknown = false ) const;
        bool GetUINT( const char* caller, const unsigned int& val, unsigned int& out, const std::string& name = "UINT" ) const;

        // checks if condition action exists before calling it
        ScriptEditReturn CallEditIf( const ScriptCode& code );
        ScriptEditReturn CallEditIf( const ScriptCode& code, const std::string& name, std::vector<std::string> values = std::vector<std::string>() );

        // checks if result action exists before calling it
        ScriptEditReturn CallEditDo( ScriptCode& code );
        ScriptEditReturn CallEditDo( ScriptCode& code, const std::string& name, std::vector<std::string> values = std::vector<std::string>() );
    };

    struct ScriptFile
    {
        std::vector<std::string> Defines;
    };

    struct ScriptCode
    {
        enum class Format : unsigned char
        {
            UNCHANGED = 0, // func(preserve, original,formatting )
            WIDE,          // func( spaces, added, everywhere )
            MEDIUM,        // func(spaces, between, arguments)
            TIGHT,         // func(no,spaces,allowed)

            MIN     = UNCHANGED,
            MAX     = TIGHT,
            DEFAULT = MEDIUM
        };

        // internal flags set for all ScriptCode
        enum class Flag : unsigned char
        {
            NONE     = 0,

            // types

            RESERVED = 0x01,
            VARIABLE = 0x02,  // set when code looks like a variable
            FUNCTION = 0x04,  // set when code looks like a function

            //

            EDITED   = 0x10, // set if any result function has been executed
            REFRESH  = 0x20, // set when code needs standard processing between edits
        };

        struct Argument
        {
            std::string Raw; // original
            std::string Arg; // trimmed
            std::string Type;
        };

        // always set

        ReDefine*             Parent;
        ScriptFile*           File;

        // dynamic

        Flag                  Flags;
        std::string           Full;
        std::string           Name;             // used by variables/functions
        std::string           ReturnType;       // used by variables/functions
        std::vector<Argument> Arguments;        // used by functions
        std::string           Operator;         // used by variables/functions
        std::string           OperatorArgument; // used by variables/functions

        // changelog
        // keeps history of of all code changes

        std::vector<std::pair<std::string, std::string>> Changes;

        //

        ScriptCode( const ScriptCode::Flag& flags = ScriptCode::Flag::NONE );

        bool IsFlag( ScriptCode::Flag flag ) const;
        void SetFlag( ScriptCode::Flag flag );
        void UnsetFlag( ScriptCode::Flag flag );
        void SetType( const ScriptCode::Flag& type );

        // returns string representation of ScriptCode
        std::string GetFullString() const;

        // sets ScriptCode::Full to value returned by GetFullString()
        void SetFullString();

        void SetFunctionArgumentsTypes( const FunctionProto& proto );

        // helpers

        bool IsVariable( const char* caller ) const;
        bool IsFunction( const char* caller ) const;
        bool IsFunctionKnown( const char* caller ) const;
        bool IsVariableOrFunction( const char* caller ) const;

        void Change( const std::string& left, const std::string& right = std::string() );
        void ChangeLog();
    };

    enum class ScriptDebugChanges : unsigned char
    {
        NONE = 0,
        ONLY_IF_CHANGED,
        ALL,

        MIN  = NONE,
        MAX  = ALL
    };

    std::map<std::string, ScriptEditIf>             EditIf;
    std::map<std::string, ScriptEditDo>             EditDo;
    std::map<unsigned int, std::vector<ScriptEdit>> EditBefore;
    std::map<unsigned int, std::vector<ScriptEdit>> EditAfter;
    std::map<unsigned int, std::vector<ScriptEdit>> Edit______;

    ScriptDebugChanges                              DebugChanges;
    bool                                            UseParser;
    bool                                            ScriptFormattingForced;
    bool                                            ScriptFormattingUnix;
    ScriptCode::Format                              ScriptFormatting;

    void InitScript();
    void FinishScript( bool finishCallbacks = true );

    bool ReadConfigScript( const std::string& sectionPrefix );


    //

    void ProcessScript( const std::string& path, const std::string& filename, const bool readOnly = false );
    void ProcessScriptReplacements( ScriptCode& code, bool refresh = false );
    void ProcessScriptEdit( const ScriptEditAction::Flag& initFlag, const std::map<unsigned int, std::vector<ScriptEdit>>& edits, ScriptCode& code, bool& restart );

    //
    // Text
    //

    bool                     TextIsBlank( const std::string& text );
    bool                     TextIsComment( const std::string& text );
    bool                     TextIsInt( const std::string& text );
    bool                     TextIsConflict( const std::string& text );
    std::string              TextGetFilename( const std::string& path, const std::string& filename );
    bool                     TextGetInt( const std::string& text, int& result, const unsigned char& base = 10 );
    std::string              TextGetJoined( const std::vector<std::string>& text, const std::string& delimeter );
    std::string              TextGetLower( const std::string& text );
    std::string              TextGetPacked( const std::string& text );
    std::string              TextGetReplaced( const std::string& text, const std::string& from, const std::string& to );
    std::vector<std::string> TextGetSplitted( const std::string& text, const char& separator );

    std::string TextGetTrimmed( const std::string& text );

    bool       TextIsDefine( const std::string& text );
    bool       TextGetDefineInt( const std::string& text, const std::regex& re, std::string& name, int& value );
    bool       TextGetDefineString( const std::string& text, const std::regex& re, std::string& name, std::string& value );
    std::regex TextGetDefineIntRegex( std::string prefix, std::string suffix, bool paren );
    std::regex TextGetDefineStringRegex( std::string prefix, std::string suffix, bool paren, const std::string& re );

    unsigned int TextGetVariables( const std::string& text, std::vector<ScriptCode>& result );
    unsigned int TextGetFunctions( const std::string& text, std::vector<ScriptCode>& result );

    //
    // Variables
    //

    std::map<std::string, std::string> VariablesPrototypes;
    std::vector<std::string>           VariablesGuessing;  // <types>

    void FinishVariables();

    bool ReadConfigVariables( const std::string& section );
};
