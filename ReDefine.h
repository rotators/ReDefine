#ifndef __REDEFINE__
#define __REDEFINE__    // :)

#include <functional>
#include <map>
#include <regex>
#include <string>
#include <tuple>
#include <vector>

class Ini;

class ReDefine
{
public:

    struct ScriptCode;

    //
    // misc maps
    //

    typedef std::map<std::string, std::map<int, std::string>>          DefinesMap;
    typedef std::map<std::string, std::map<std::string, std::string>>  GenericOperatorsMap;
    typedef std::map<std::string, std::vector<std::string>>            StringVectorMap;
    typedef std::map<std::string, std::map<std::string, unsigned int>> CountersMap;

    //
    // script edit actions
    //

    typedef std::function<bool (const ScriptCode&, const std::vector<std::string>&)> ScriptEditIf;
    typedef std::function<bool (ScriptCode&, const std::vector<std::string>&)>       ScriptEditDo;

    //
    // ReDefine
    //

    Ini* Config;

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

    bool ReadFile( const std::string& filename, std::vector<std::string>& lines );
    bool ReadFile( const std::string& filename, std::vector<char>& letters );
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

    StringVectorMap     FunctionsArguments; // <name, <types>>
    GenericOperatorsMap FunctionsOperators; // <name, <operatorName, type>>

    void FinishFunctions();

    bool ReadConfigFunctions( const std::string& sectionPrefix );

    void ProcessFunctionArguments( ScriptCode& function );

    //
    // Log
    //

    void DEBUG( const char* caller, const char* format, ... );
    void WARNING( const char* caller, const char* format, ... );
    void ILOG( const char* format, ... );
    void LOG( const char* format, ... );

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

    void ProcessOperator( const GenericOperatorsMap& map, ScriptCode& code );

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

    struct ScriptFile
    {
        std::vector<std::string> Defines;
    };

    enum ScriptCodeFlag : unsigned int
    {
        // types

        SCRIPT_CODE_VARIABLE = 0x01,
        SCRIPT_CODE_FUNCTION = 0x02,

        //

        SCRIPT_CODE_EDITED   = 0x10, // set if any result function has been executed
        SCRIPT_CODE_REFRESH  = 0x20, // set when code needs standard processing between edits
        SCRIPT_CODE_RESTART  = 0x40  // set by DoRestart
    };

    struct ScriptCode
    {
        ReDefine*                Parent;
        ScriptFile*              File;

        unsigned int             Flags;
        std::string              Full; // Name + (Arguments) + (Operator + OperatorArguments)
        std::string              Name;
        std::vector<std::string> Arguments;
        std::vector<std::string> ArgumentsTypes;
        std::string              Operator;
        std::string              OperatorArgument;

        ScriptCode( const unsigned int& flags = 0 );

        bool IsFlag( unsigned int flag ) const;
        void SetFlag( unsigned int flag );
        void UnsetFlag( unsigned int flag );
        void SetType( const ScriptCodeFlag& type );

        // returns string representation of ScriptCode
        std::string GetFullString() const;

        // sets ScriptCode::Full to value returned by GetFullString()
        void SetFullString();

        // helpers

        bool IsVariable( const char* caller ) const;
        bool IsFunction( const char* caller ) const;
        bool IsFunctionKnown( const char* caller ) const;
        bool IsVariableOrFunction( const char* caller ) const;
        bool IsValues( const char* caller, const std::vector<std::string>& values, const uint& count ) const;

        bool GetINDEX( const char* caller, const std::string& value, unsigned int& val ) const;
        bool GetTYPE( const char* caller, const std::string& value, bool allowUnknown = false ) const;
        bool GetUINT( const char* caller, const std::string& value, unsigned int& val, const std::string& name = "UINT" ) const;

        // checks if condition action exists before calling it
        bool CallEditIf( const std::string& name, std::vector<std::string> values = std::vector<std::string>() ) const;

        // checks if result action exists before calling it
        bool CallEditDo( const std::string& name, std::vector<std::string> values = std::vector<std::string>() );
    };

    struct ScriptEdit
    {
        struct Action
        {
            std::string              Name;
            std::vector<std::string> Values;
        };

        bool                Debug;
        std::string         Name;

        std::vector<Action> Conditions;
        std::vector<Action> Results;

        ScriptEdit();
    };


    std::map<std::string, ScriptEditIf>             EditIf;
    std::map<std::string, ScriptEditDo>             EditDo;
    std::map<unsigned int, std::vector<ScriptEdit>> EditBefore;
    std::map<unsigned int, std::vector<ScriptEdit>> EditAfter;
    bool                                            EditDebug;

    void InitScript();
    void FinishScript( bool finishCallbacks = true );

    bool ReadConfigScript( const std::string& sectionPrefix );


    //

    void ProcessScript( const std::string& path, const std::string& filename, const bool readOnly = false );
    void ProcessScriptVariable( ScriptCode& code );
    void ProcessScriptFunction( ScriptCode& code );
    void ProcessScriptEdit( const std::string& info, const std::map<unsigned int, std::vector<ScriptEdit>>& edits, ScriptCode& code );
    void ProcessScriptEditChangelog( const std::vector<std::pair<std::string, std::string>>& changelog );

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
    bool       TextGetDefine( const std::string& text, const std::regex& re, std::string& name, int& value );
    std::regex TextGetDefineRegex( std::string prefix, std::string suffix, bool paren );

    unsigned int TextGetVariables( const std::string& text, std::vector<ScriptCode>& result );
    unsigned int TextGetFunctions( const std::string& text, std::vector<ScriptCode>& result );

    //
    // Variables
    //

    GenericOperatorsMap      VariablesOperators; // <name, <operatorName, type>>
    std::vector<std::string> VariablesGuessing;  // <types>

    void FinishVariables();

    bool ReadConfigVariables( const std::string& sectionPrefix );
};

#endif // __REDEFINE__ //
