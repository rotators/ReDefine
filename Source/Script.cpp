#include <chrono>
#include <fstream>

#include "FOClassic/Ini.h"

#include "ReDefine.h"
#include "StdFilesystem.h"
#if HAVE_PARSER
# include "Parser.h"
#endif

ReDefine::ScriptCode::ScriptCode( const unsigned int& flags /* = 0 */ ) :
    Parent( nullptr ),
    File( nullptr ),
    Flags( flags ),
    Full( "" ),
    Name( "" ),
    ReturnType( "" ),
    Arguments(),
    ArgumentsTypes(),
    Operator( "" ),
    OperatorArgument( "" )
{}

bool ReDefine::ScriptCode::IsFlag( unsigned int flag ) const
{
    return (Flags & flag) != 0;
}

void ReDefine::ScriptCode::SetFlag( unsigned int flag )
{
    Flags = Flags | flag;
}

void ReDefine::ScriptCode::UnsetFlag( unsigned int flag )
{
    Flags = (Flags | flag) ^ flag;
}

void ReDefine::ScriptCode::SetType( const ReDefine::ScriptCodeFlag& type )
{
    UnsetFlag( SCRIPT_CODE_VARIABLE );
    UnsetFlag( SCRIPT_CODE_FUNCTION );

    SetFlag( type );
}

//

std::string ReDefine::ScriptCode::GetFullString() const
{
    std::string result;

    if( IsFunction( nullptr ) || IsVariable( nullptr ) )
    {
        if( Name.empty() )
            return std::string();

        result = Name;

        if( IsFunction( nullptr ) )
        {
            result += "(";

            if( !Arguments.empty() )
            {
                switch( Parent->ScriptFormatting )
                {
                    case SCRIPT_FORMAT_UNCHANGED:
                        result += Parent->TextGetJoined( ArgumentsRaw, "," );
                        break;
                    case SCRIPT_FORMAT_WIDE:
                        result += " " + Parent->TextGetJoined( Arguments, ", " ) + " ";
                        break;
                    case SCRIPT_FORMAT_MEDIUM:
                        result += Parent->TextGetJoined( Arguments, ", " );
                        break;
                    case SCRIPT_FORMAT_TIGHT:
                        result += Parent->TextGetJoined( Arguments, "," );
                        break;
                    default:
                        Parent->WARNING( __FUNCTION__, "unknown formatting<%u> : switching to default<%u>", Parent->ScriptFormatting, SCRIPT_FORMAT_WIDE );
                        Parent->ScriptFormatting = SCRIPT_FORMAT_WIDE;
                        result += " " + Parent->TextGetJoined( Arguments, ", " ) + " ";
                }
            }

            result += ")";
        }

        if( !Operator.empty() && !OperatorArgument.empty() )
        {
            result += " " + Operator + " " + OperatorArgument;
        }
    }

    return result;
}

void ReDefine::ScriptCode::SetFullString()
{
    Full = GetFullString();
}

//

bool ReDefine::ScriptCode::IsBefore( const char* caller ) const
{
    if( !IsFlag( SCRIPT_CODE_BEFORE ) )
    {
        if( caller )
            Parent->WARNING( caller, "action can be used only in RunBefore edits" );

        return false;
    }

    return true;
}

bool ReDefine::ScriptCode::IsAfter( const char* caller ) const
{
    if( !IsFlag( SCRIPT_CODE_AFTER ) )
    {
        if( caller )
            Parent->WARNING( caller, "action can be used only in RunAfter edits" );

        return false;
    }

    return true;
}

bool ReDefine::ScriptCode::IsVariable( const char* caller ) const
{
    if( Name.empty() )
    {
        if( caller )
            Parent->WARNING( caller, "name not set" );

        return false;
    }

    if( !IsFlag( SCRIPT_CODE_VARIABLE ) )
    {
        if( caller )
            Parent->WARNING( caller, "script code<%s> is not a variable", Name.c_str() );

        return false;
    }

    return true;
}

bool ReDefine::ScriptCode::IsFunction( const char* caller ) const
{
    if( Name.empty() )
    {
        if( caller )
            Parent->WARNING( caller, "name not set" );

        return false;
    }

    if( !IsFlag( SCRIPT_CODE_FUNCTION ) )
    {
        if( caller )
            Parent->WARNING( caller, "script code<%s> is not a function", Name.c_str() );

        return false;
    }

    return true;
}

bool ReDefine::ScriptCode::IsFunctionKnown( const char* caller ) const
{
    if( !IsFunction( caller ) )
        return false;

    if( Parent->FunctionsPrototypes.find( Name ) == Parent->FunctionsPrototypes.end() )
    {
        if( caller )
            Parent->WARNING( caller, "function<%s> must be added to configuration before using this action", Name.c_str() );

        return false;
    }

    return true;
}

bool ReDefine::ScriptCode::IsVariableOrFunction( const char* caller ) const
{
    if( Name.empty() )
    {
        if( caller )
            Parent->WARNING( caller, "name not set" );

        return false;
    }

    if( !IsFlag( SCRIPT_CODE_VARIABLE ) && !IsFlag( SCRIPT_CODE_FUNCTION ) )
    {
        if( caller )
            Parent->WARNING( caller, "script code<%s> is not a variable or function", Name.c_str() );

        return false;
    }

    return true;
}

bool ReDefine::ScriptCode::IsValues( const char* caller, const std::vector<std::string>& values, const unsigned int& count ) const
{
    if( values.size() < count )
    {
        if( caller )
            Parent->WARNING( caller, "wrong number of arguments : required<%u> provided<%u>", count, values.size() );

        return false;
    }

    for( unsigned int c = 0; c < count; c++ )
    {
        if( values[c].empty() )
        {
            if( caller )
                Parent->WARNING( caller, "wrong number of arguments : argument<%u> is empty", c + 1 );

            return false;
        }
    }

    return true;
}

//

bool ReDefine::ScriptCode::GetINDEX( const char* caller, const std::string& value, unsigned int& val ) const
{
    unsigned int tmp;
    if( !GetUINT( caller, value, tmp, "INDEX" ) )
        return false;

    if( tmp >= Arguments.size() )
    {
        if( caller )
            Parent->WARNING( caller, "INDEX<%u> out of range", tmp );

        return false;
    }

    val = tmp;
    return true;
}

bool ReDefine::ScriptCode::GetTYPE( const char* caller, const std::string& value, bool allowUnknown /* = false */ ) const
{
    if( value.empty() )
    {
        if( caller )
            Parent->WARNING( caller, "TYPE is empty" );

        return false;
    }

    if( allowUnknown && Parent->IsMysteryDefineType( value ) )
        return true;

    if( !Parent->IsDefineType( value ) )
    {
        if( caller )
            Parent->WARNING( caller, "unknown TYPE<%s>", value.c_str() );

        return false;
    }

    return true;
}

bool ReDefine::ScriptCode::GetUINT( const char* caller, const std::string& value, unsigned int& val, const std::string& name /* = "UINT" */ ) const
{
    int tmp = -1;
    if( !Parent->TextIsInt( value ) || !Parent->TextGetInt( value, tmp ) )
    {
        if( caller )
            Parent->WARNING( caller, "invalid %s<%s>", name.c_str(), value.c_str() );

        return false;
    }

    if( tmp < 0 )
    {
        if( caller )
            Parent->WARNING( caller, "invalid %s<%s> : value < 0", name.c_str(), value.c_str() );

        return false;
    }

    val = tmp;
    return true;
}


// checks if given edit action exists before trying to call it
// as EditIf/EditDo should be free to modify by main application at any point,
// it can use EditIf["IfThing"](...) like there's no tomorrow, but ReDefine class doing same thing is Bad Idea (TM)

bool ReDefine::ScriptCode::CallEditIf( const std::string& name, std::vector<std::string> values /* = std::vector<std::string>() */ ) const
{
    auto it = Parent->EditIf.find( name );
    if( it == Parent->EditIf.end() )
    {
        Parent->WARNING( __FUNCTION__, "script action condition<%s> not found", name.c_str() );
        return false;
    }

    return it->second( *this, values );
}

bool ReDefine::ScriptCode::CallEditDo( const std::string& name, std::vector<std::string> values /* = std::vector<std::string>() */ )
{
    auto it = Parent->EditDo.find( name );
    if( it == Parent->EditDo.end() )
    {
        Parent->WARNING( __FUNCTION__, "script action result<%s> not found", name.c_str() );
        return false;
    }

    return it->second( *this, values );
}

void ReDefine::ScriptCode::Change( const std::string& left, const std::string& right /* = std::string() */ )
{
    Changes.push_back( std::make_pair( left, right ) );
}

void ReDefine::ScriptCode::ChangeLog()
{
    size_t max = 0;
    for( const auto& change : Changes )
    {
        if( change.first.length() > max )
            max = change.first.length();
    }

    SStatus::SCurrent previous = Parent->Status.Current;
    Parent->Status.Current.Clear();

    for( const auto& change : Changes )
    {
        if( !change.second.empty() )
        {
            std::string dots = std::string( (max - change.first.length() ) + 3, '.' );
            Parent->DEBUG( nullptr, "%s %s %s", change.first.c_str(), dots.c_str(), change.second.c_str() );
        }
        else
            Parent->DEBUG( nullptr, "%s", change.first.c_str() );
    }

    Parent->Status.Current = previous;
}

//

ReDefine::ScriptEdit::ScriptEdit() :
    Debug( false ),
    Name( "" ),
    Conditions(),
    Results()
{}

// script edit conditions

// ? IfArgumentIs:INDEX,DEFINE
// ? IfArgumentIs:INDEX,DEFINE,TYPE
static bool IfArgumentIs( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunctionKnown( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    if( code.Parent->TextIsInt( code.Arguments[idx] ) )
    {
        int         val = -1;
        std::string type = code.ArgumentsTypes[idx], value;

        if( code.IsValues( nullptr, values, 3 ) )
        {
            if( code.GetTYPE( __FUNCTION__, values[2] ) )
                type = values[2];
            else
                return false;
        }

        if( !code.Parent->TextGetInt( code.Arguments[idx], val ) || !code.Parent->GetDefineName( type, val, value ) )
            return false;

        // code.Parent->DEBUG( __FUNCTION__, "parsed compare: %s == %s", value.c_str(), values[1].c_str() );
        return value == values[1];
    }

    // code.Parent->DEBUG( __FUNCTION__, "raw compare: %s == %s", code.Arguments[idx].c_str(), values[1].c_str() );
    return code.Arguments[idx] == values[1];
}

// ? IfArgumentValue:INDEX,STRING
static bool IfArgumentValue( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    return code.Arguments[idx] == values[1];
}

// ? IfArgumentNotValue:INDEX,STRING
static bool IfArgumentNotValue( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    return code.Arguments[idx] != values[1];
}

// ? IfArgumentsEqual:INDEX,INDEX
static bool IfArgumentsEqual( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    unsigned int idx1;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx1 ) )
        return false;

    unsigned int idx2;
    if( !code.GetINDEX( __FUNCTION__, values[1], idx2 ) )
        return false;

    if( idx1 == idx2 )
    {
        code.Parent->WARNING( __FUNCTION__, "duplicated INDEX<%u>", idx1 );
        return false;
    }

    return code.Arguments[idx1] == code.Arguments[idx2];
}

// ? IfArgumentsSize:UINT
static bool IfArgumentsSize( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    unsigned int size;
    if( !code.GetUINT( __FUNCTION__, values[0], size ) )
        return false;

    return code.Arguments.size() == size;
}

// ? IfEdited
static bool IfEdited( const ReDefine::ScriptCode& code, const std::vector<std::string>& )
{
    return code.IsFlag( ReDefine::SCRIPT_CODE_EDITED );
}

// ? IfFileDefined:STRING
static bool IfFileDefined( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    return std::find( code.File->Defines.begin(), code.File->Defines.end(), values[0] ) != code.File->Defines.end();
}

// ? IfFileName:STRING
static bool IfFileName( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    return code.Parent->Status.Current.File == values[0];
}

// ? IfFunction
// ? IfFunction:STRING
// ? IfFunction:STRING_1,...,STRING_N
// > IfName
static bool IfFunction( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( nullptr ) )
        return false;

    if( code.IsValues( nullptr, values, 1 ) )
    {
        for( const auto& value : values )
        {
            if( value.empty() )
                break;

            if( code.CallEditIf( "IfName", { value } ) )
                return true;
        }

        return false;
    }

    return true;
}

// ? IfName:STRING
static bool IfName( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    return code.Name == values[0];
}

// ? IfNotEdited
static bool IfNotEdited( const ReDefine::ScriptCode& code, const std::vector<std::string>& )
{
    return !code.IsFlag( ReDefine::SCRIPT_CODE_EDITED );
}

// ? IfOperator
static bool IfOperator( const ReDefine::ScriptCode& code, const std::vector<std::string>& )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    return code.Operator.length() > 0;
}

// ? IfOperatorName:STRING
static bool IfOperatorName( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    if( code.Operator.empty() )
        return false;

    return code.Parent->GetOperatorName( code.Operator ) == values[0];
}

// ? IfOperatorValue:STRING
static bool IfOperatorValue( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    if( code.OperatorArgument.empty() )
        return false;

    return code.OperatorArgument == values[0];
}

// ? IfReturnType:TYPE
static bool IfReturnType( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    if( !code.GetTYPE( __FUNCTION__, values[0], true ) )
        return false;

    return code.ReturnType == values[0];
}

// ? IfVariable
// ? IfVariable:STRING
// ? IfVariable:STRING_1,...STRING_N
// > IfName
static bool IfVariable( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsVariable( nullptr ) )
        return false;

    if( code.IsValues( nullptr, values, 1 ) )
    {
        for( const auto& value : values )
        {
            if( code.CallEditIf( "IfName", { value } ) )
                return true;
        }

        return false;
    }

    return true;
}

// script edit results

// ? DoArgumentCount:INDEX,STRING
static bool DoArgumentCount( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    code.Parent->Status.Process.Counters[values[1]][code.Arguments[idx]]++;

    return true;
}

// ? DoArgumentSet:INDEX,STRING
static bool DoArgumentSet( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    code.ArgumentsRaw[idx] = code.Parent->TextGetReplaced( code.ArgumentsRaw[idx], code.Arguments[idx], values[1] );
    code.Arguments[idx] = values[1];

    return true;
}

// ? DoArgumentSetPrefix:INDEX,STRING
// > DoArgumentSet
static bool DoArgumentSetPrefix( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    return code.CallEditDo( "DoArgumentSet", { values[0], values[1] + code.Arguments[idx] } );
}

// ? DoArgumentSetSuffix:INDEX,STRING
// > DoArgumentSet
static bool DoArgumentSetSuffix( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    return code.CallEditDo( "DoArgumentSet", { values[0], code.Arguments[idx] + values[1] } );
}

// ? DoArgumentSetType:INDEX,TYPE
static bool DoArgumentSetType( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsBefore( __FUNCTION__ ) )
        return false;

    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    code.ArgumentsTypes[idx] = values[1];

    return true;
}

// ? DoArgumentLookup:INDEX
// ? DoArgumentLookup:INDEX,STRING
static bool DoArgumentLookup( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    bool        unknown = false;
    std::string counter;

    if( code.IsValues( nullptr, values, 2 ) )
    {
        if( values[1] == "!UNKNOWN!" )
            unknown = true;
        else
            counter = values[1];
    }

    if( code.Parent->TextIsInt( code.Arguments[idx] ) )
    {
        int         val = -1;
        std::string value;

        if( !code.Parent->TextGetInt( code.Arguments[idx], val ) )
        {
            code.Parent->WARNING( __FUNCTION__, "cannot convert string<%s> into integer", code.Arguments[idx].c_str() );
            return true;
        }

        if( !code.Parent->GetDefineName( code.Arguments[idx], val, value ) )
        {
            code.Parent->WARNING( __FUNCTION__, "unknown %s<%s>", code.ArgumentsTypes[idx].c_str(), code.Arguments[idx].c_str() );

            if( unknown )
                code.Parent->Status.Process.Counters["!Unknown " + code.ArgumentsTypes[idx] + "!"][code.Arguments[idx]]++;
            else if( !counter.empty() )
                code.Parent->Status.Process.Counters[counter][code.Arguments[idx]]++;

            return true;
        }
    }
    else
    {
        int val = -1;
        if( !code.Parent->GetDefineValue( code.ArgumentsTypes[idx], code.Arguments[idx], val ) )
        {
            code.Parent->WARNING( __FUNCTION__, "unknown %s<%s>", code.ArgumentsTypes[idx].c_str(), code.Arguments[idx].c_str() );

            if( unknown )
                code.Parent->Status.Process.Counters["!Unknown " + code.ArgumentsTypes[idx] + "!"][code.Arguments[idx]]++;
            else if( !counter.empty() )
                code.Parent->Status.Process.Counters[counter][code.Arguments[idx]]++;

            return true;
        }
    }

    return true;
}

// ? DoArgumentsClear
static bool DoArgumentsClear( ReDefine::ScriptCode& code, const std::vector<std::string>& )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    code.Arguments.clear();
    code.ArgumentsRaw.clear();
    code.ArgumentsTypes.clear();

    return true;
}

// ? DoArgumentsErase:INDEX
static bool DoArgumentsErase( ReDefine::ScriptCode& code, const std::vector<std::string>& values  )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    code.Arguments.erase( code.Arguments.begin() + idx );
    code.ArgumentsRaw.erase( code.ArgumentsRaw.begin() + idx );
    code.ArgumentsTypes.erase( code.ArgumentsTypes.begin() + idx );

    return true;
}

// ? DoArgumentsMoveBack:INDEX
// > DoArgumentsErase
// > DoArgumentsPushBack
static bool DoArgumentsMoveBack( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    std::string arg = code.Arguments[idx], type = code.ArgumentsTypes[idx];

    if( !code.CallEditDo( "DoArgumentsErase", { values[0] } ) )
        return false;

    if( !code.CallEditDo( "DoArgumentsPushBack", { arg, type } ) )
        return false;

    return true;
}

// ? DoArgumentsMoveFront:INDEX
// > DoArgumentsErase
// > DoArgumentsPushFront
static bool DoArgumentsMoveFront( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    std::string argument = code.Arguments[idx], type = code.ArgumentsTypes[idx];

    if( !code.CallEditDo( "DoArgumentsErase", { values[0] } ) )
        return false;

    if( !code.CallEditDo( "DoArgumentsPushFront", { argument, type } ) )
        return false;

    return true;
}

// ? DoArgumentsPushBack:STRING
// ? DoArgumentsPushBack:STRING,TYPE
static bool DoArgumentsPushBack( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    std::string type = "?";

    if( code.IsValues( nullptr, values, 2 ) )
        type = values[1];

    if( !code.GetTYPE( __FUNCTION__, type, true ) )
        return false;

    code.Arguments.push_back( values[0] );
    code.ArgumentsRaw.push_back( values[0] );
    code.ArgumentsTypes.push_back( type );

    return true;
}

// ? DoArgumentsPushFront:STRING,
// ? DoArgumentsPushFront:STRING,TYPE
static bool DoArgumentsPushFront( ReDefine::ScriptCode& code, const std::vector<std::string>& values  )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    std::string type = "?";

    if( code.IsValues( nullptr, values, 2 ) )
        type = values[1];

    if( !code.GetTYPE( __FUNCTION__, type, true ) )
        return false;

    code.Arguments.insert( code.Arguments.begin(), values[0] );
    code.ArgumentsRaw.insert( code.ArgumentsRaw.begin(), values[0] );
    code.ArgumentsTypes.insert( code.ArgumentsTypes.begin(), type );

    return true;
}

// ? DoArgumentsResize:UINT
// > DoArgumentsClear
static bool DoArgumentsResize( ReDefine::ScriptCode& code, const std::vector<std::string>& values  )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    unsigned int size;
    if( !code.GetUINT( __FUNCTION__, values[0], size ) )
        return false;

    if( !size )
        return code.CallEditDo( "DoArgumentsClear", {} );

    code.Arguments.resize( size );
    code.ArgumentsRaw.resize( size );
    code.ArgumentsTypes.resize( size );

    return true;
}

// ? DoFileCount:STRING
static bool DoFileCount( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    code.Parent->Status.Process.Counters[values[0]][code.Parent->Status.Current.File]++;

    return true;
}

// ? DoFunction
// ? DoFunction:STRING
// > DoNameSet
static bool DoFunction( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    code.SetType( ReDefine::SCRIPT_CODE_FUNCTION );

    if( code.IsValues( nullptr, values, 1 ) )
    {
        if( !code.CallEditDo( "DoNameSet", { values[0] } ) )
            return false;
    }

    code.SetFlag( ReDefine::SCRIPT_CODE_REFRESH );

    return true;
}

// ? DoLogCurrentLine
static bool DoLogCurrentLine( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    ReDefine::SStatus::SCurrent previous = code.Parent->Status.Current;
    code.Parent->Status.Current.Line.clear();

    if( !values.empty() && !values[0].empty() )
    {

        if( values[0] == "DEBUG" )
        {
            code.Parent->DEBUG( nullptr, "%%%%" );
            code.Parent->Status.Current.Clear();
            code.Parent->DEBUG( nullptr, "   %s", previous.Line.c_str() );
        }
        else if( values[0] == "WARNING" )
        {
            code.Parent->WARNING( nullptr, "%%%%" );
            code.Parent->Status.Current.Clear();
            code.Parent->WARNING( nullptr, "   %s", previous.Line.c_str() );
        }

    }
    else
    {
        code.Parent->ILOG( "%%%%" );
        code.Parent->LOG( "   %s", previous.Line.c_str() );
    }

    code.Parent->Status.Current = previous;

    return true;
}

// ? DoNameCount:STRING
static bool DoNameCount( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    code.Parent->Status.Process.Counters[values[0]][code.Name]++;

    return true;
}

// ? DoNameSet:STRING
static bool DoNameSet( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    code.Name = values[0];
    code.SetFlag( ReDefine::SCRIPT_CODE_REFRESH );

    return true;
}

// ? DoOperatorClear
static bool DoOperatorClear( ReDefine::ScriptCode& code, const std::vector<std::string>& )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    code.Operator.clear();
    code.OperatorArgument.clear();

    return true;
}

// ? DoOperatorSet:STRING,STRING
static bool DoOperatorSet( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 2 ) )
        return false;

    code.Operator = values[0];
    code.OperatorArgument = values[1];

    return true;
}

// ? DoRestart
static bool DoRestart( ReDefine::ScriptCode& code, const std::vector<std::string>& )
{
    code.SetFlag( ReDefine::SCRIPT_CODE_RESTART );

    return true;
}

// ? DoReturnSetType:TYPE
static bool DoReturnSetType( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsBefore( __FUNCTION__ ) )
        return false;

    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return false;

    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    if( !code.GetTYPE( __FUNCTION__, values[0] ) )
        return false;

    code.ReturnType = values[0];

    return true;
}

// ? DoVariable
// ? DoVariable:STRING
// > DoArgumentsClear
// > DoNameSet
static bool DoVariable( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( code.IsFunction( nullptr ) )
    {
        if( !code.CallEditDo( "DoArgumentsClear" ) )
            return false;
    }

    code.SetType( ReDefine::SCRIPT_CODE_VARIABLE );

    if( code.IsValues( nullptr, values, 1 ) )
    {
        if( !code.CallEditDo( "DoNameSet", { values[0] } ) )
            return false;
    }

    code.SetFlag( ReDefine::SCRIPT_CODE_REFRESH );

    return true;
}

//

void ReDefine::InitScript()
{
    // must start with "If"
    EditIf["IfArgumentIs"] = &IfArgumentIs;
    EditIf["IfArgumentNotValue"] = &IfArgumentNotValue;
    EditIf["IfArgumentValue"] = &IfArgumentValue;
    EditIf["IfArgumentsEqual"] = &IfArgumentsEqual;
    EditIf["IfArgumentsSize"] = &IfArgumentsSize;
    EditIf["IfEdited"] = &IfEdited;
    EditIf["IfFileDefined"] = &IfFileDefined;
    EditIf["IfFileName"] = &IfFileName;
    EditIf["IfFunction"] = &IfFunction;
    EditIf["IfName"] = &IfName;
    EditIf["IfNotEdited"] = &IfNotEdited;
    EditIf["IfOperator"] = &IfOperator;
    EditIf["IfOperatorName"] = &IfOperatorName;
    EditIf["IfOperatorValue"] = &IfOperatorValue;
    EditIf["IfReturnType"] = &IfReturnType;
    EditIf["IfVariable"] = &IfVariable;

    // must start with "Do"
    EditDo["DoArgumentCount"] = &DoArgumentCount;
    EditDo["DoArgumentLookup"] = &DoArgumentLookup;
    EditDo["DoArgumentSet"] = &DoArgumentSet;
    EditDo["DoArgumentSetPrefix"] = &DoArgumentSetPrefix;
    EditDo["DoArgumentSetSuffix"] = &DoArgumentSetSuffix;
    EditDo["DoArgumentSetType"] = &DoArgumentSetType;
    EditDo["DoArgumentsClear"] = &DoArgumentsClear;
    EditDo["DoArgumentsErase"] = &DoArgumentsErase;
    EditDo["DoArgumentsMoveBack"] = &DoArgumentsMoveBack;
    EditDo["DoArgumentsMoveFront"] = &DoArgumentsMoveFront;
    EditDo["DoArgumentsPushBack"] = &DoArgumentsPushBack;
    EditDo["DoArgumentsPushFront"] = &DoArgumentsPushFront;
    EditDo["DoArgumentsResize"] = &DoArgumentsResize;
    EditDo["DoFileCount"] = &DoFileCount;
    EditDo["DoFunction"] = &DoFunction;
    EditDo["DoLogCurrentLine"] = &DoLogCurrentLine;
    EditDo["DoNameCount"] = &DoNameCount;
    EditDo["DoNameSet"] = &DoNameSet;
    EditDo["DoOperatorClear"] = &DoOperatorClear;
    EditDo["DoOperatorSet"] = &DoOperatorSet;
    EditDo["DoRestart"] = &DoRestart;
    EditDo["DoReturnSetType"] = &DoReturnSetType;
    EditDo["DoVariable"] = &DoVariable;
}

void ReDefine::FinishScript( bool finishCallbacks /* = true */ )
{
    if( finishCallbacks )     // we kinda need them between config resets :)
    {
        EditIf.clear();
        EditDo.clear();
    }

    EditBefore.clear();
    EditAfter.clear();

    DebugChanges = UseParser = false;
}

// reading

bool ReDefine::ReadConfigScript( const std::string& sectionPrefix )
{
    FinishScript( false );

    std::vector<std::string> sections;
    if( !Config->GetSections( sections ) )
        return false;

    for( const auto& section : sections )
    {
        // [???] //
        if( section.substr( 0, sectionPrefix.length() ) != sectionPrefix )
            continue;
        // [Script] or [Script:CATEGORY] //
        else if( section == sectionPrefix || (section.length() >= sectionPrefix.length() + 2 && section.substr( sectionPrefix.length(), 1 ) == ":") )
        {
            // DEBUG( nullptr, "section<%s>", section.c_str() );

            std::string category;
            if( section != sectionPrefix )
                category = section.substr( sectionPrefix.length() + 1 ) + "->";

            std::vector<std::string> keys;
            if( !Config->GetSectionKeys( section, keys, true ) )
                continue;

            for( const auto& name : keys )
            {
                if( Config->IsSectionKeyEmpty( section, name ) )
                    continue;

                ScriptEdit edit;
                edit.Name = category + name;

                bool         ignore = false, before = false, after = false;
                unsigned int priority = 100;

                for( const auto& action : Config->GetStrVec( section, name ) )
                {
                    if( action.empty() )
                        continue;

                    // split name from arguments
                    std::vector<std::string> vals;
                    std::vector<std::string> arg = TextGetSplitted( action, ':' );
                    if( !arg.size() )
                    {
                        DEBUG( __FUNCTION__, "IGNORE [%s]", action.c_str() );
                        ignore = true;
                        break;
                    }
                    else if( arg.size() == 1 )
                    {}
                    else if( arg.size() == 2 )
                        vals = TextGetSplitted( arg[1], ',' );
                    else
                    {
                        WARNING( __FUNCTION__, "script edit<%s> ignored : invalid action<%s>", name.c_str(), action.c_str() );
                        ignore = true;
                        break;
                    }

                    // DEBUG( __FUNCTION__, "%s -> [%s]=[%s]", name.c_str(), arg[0].c_str(), TextGetJoined( vals, "|" ).c_str() );

                    if( arg[0] == "IGNORE" )
                    {
                        ignore = true;
                        break;
                    }
                    else if( arg[0] == "DEBUG" )
                        edit.Debug = true;
                    else if( arg[0] == "RunBefore" )
                        before = true;
                    else if( arg[0] == "RunAfter" )
                        after = true;
                    else if( arg[0].length() >= 3 && arg[0].substr( 0, 2 ) == "If" )
                    {
                        ScriptEdit::Action condition;
                        condition.Name = arg[0];
                        condition.Values = vals;

                        edit.Conditions.push_back( condition );
                    }
                    else if( arg[0].length() >= 3 && arg[0].substr( 0, 2 ) == "Do" )
                    {
                        ScriptEdit::Action result;
                        result.Name = arg[0];
                        result.Values = vals;

                        edit.Results.push_back( result );
                    }
                    else
                    {
                        WARNING( __FUNCTION__, "script edit<%s> ignored : unknown action<%s>", name.c_str(), arg[0].c_str() );
                        ignore = true;
                        break;
                    }

                    if( (arg[0] == "RunBefore" || arg[0] == "RunAfter") && vals.size() && !vals[0].empty() )
                    {
                        int tmpriority;
                        if( !TextGetInt( vals[0], tmpriority ) || tmpriority < 0 )
                        {
                            WARNING( __FUNCTION__, "script edit<%s> ignored : invalid priority<%s>", vals[0] );
                            ignore = true;
                            break;
                        }

                        priority = tmpriority;
                    }
                }

                // DEBUG( __FUNCTION__, "%s : before<%s> after<%s> condition<%u>, result<%u>", edit.Name.c_str(), edit.Before ? "true" : "false", edit.After ? "true" : "false", edit.Conditions.size(), edit.Results.size() );

                // "IGNORE" or error
                if( ignore )
                    continue;

                // validation

                if( !before && !after )
                {
                    WARNING( nullptr, "script edit<%s> ignored : at least one of 'RunBefore' or 'RunAfter' must be set", edit.Name.c_str() );
                    ignore = true;
                }

                if( !edit.Conditions.size() )
                {
                    WARNING( nullptr, "script edit<%s> ignored : no conditions", edit.Name.c_str() );
                    ignore = true;
                }

                if( !edit.Results.size() )
                {
                    WARNING( nullptr, "script edit<%s> ignored : no results", edit.Name.c_str() );
                    ignore = true;
                }

                if( ignore )
                    continue;

                if( before )
                    EditBefore[priority].push_back( edit );
                if( after )
                    EditAfter[priority].push_back( edit );
            }
        }
    }

    return true;
}

// processing

void ReDefine::ProcessScript( const std::string& path, const std::string& filename, const bool readOnly /* = false */ )
{
    if( path.empty() )
    {
        WARNING( __FUNCTION__, "script<%s> path is empty", filename.c_str() );
        return;
    }
    else if( !std_filesystem::exists( path ) )
    {
        WARNING( __FUNCTION__, "script<%s> path<%s> does not exists", filename.c_str(), path.c_str() );
        return;
    }
    else if( !std_filesystem::is_directory( path ) )
    {
        WARNING( __FUNCTION__, "script<%s> path<%s> is not a directory", filename.c_str(), path.c_str() );
        return;
    }

    std::vector<std::string> lines;
    if( !ReadFile( TextGetFilename( path, filename ), lines ) )
        return;

    #if defined (HAVE_PARSER)
    if( UseParser )
    {
        class Loader : public Parser::FileLoader
        {
public:
            ReDefine*                                Parent;
            std::string                              Path;

            std::map<std::string, std::vector<char>> DataMap;

            Loader( ReDefine* parent, const std::string& path ) : FileLoader(), Parent( parent ), Path( path )
            {
                Parent->DEBUG( nullptr, "Loader init" );
            }
            virtual ~Loader()
            {
                Parent->DEBUG( nullptr, "Loader finish" );
            }

            virtual bool Load( const std::string& filename, std::vector<char>& data ) override
            {
                auto it = DataMap.find( filename );
                if( it != DataMap.end() )
                {
                    data = it->second;
                    return true;
                }
                else if( Parent->ReadFile( Parent->TextGetFilename( Path, filename ), data ) )
                {
                    DataMap[filename] = data;
                    return true;
                }

                return false;
            };
        };

        static Loader* loader = new Loader( this, path );

        Parser         parser;
        parser.Log.Enabled = true;
        parser.Log.Cache = true;
        parser.Loader = loader;

        Parser::File file;

        // show time!
        auto              read = std::chrono::system_clock::now();
        std::vector<char> data;
        if( !ReadFile( TextGetFilename( path, filename ), data ) )
            return;

        auto explode = std::chrono::system_clock::now();
        file.Exploded = parser.Explode( filename, data );

        auto tokenize = std::chrono::system_clock::now();
        file.Tokenized = parser.Tokenize( file.Exploded );

        auto preprocess = std::chrono::system_clock::now();
        bool resultPP = parser.Preprocess( file.Tokenized, file.Preprocessed, file.Defines );

        auto parse = std::chrono::system_clock::now();
        bool resultP = parser.Parse( file.Tokenized, file.GlobalScope, file.Defines );

        auto end = std::chrono::system_clock::now();

        DEBUG( filename.c_str(), "Read(%u) Explode(%u) Tokenize(%u) Preprocess(%u) Parse(%u) Total %ums",
               /* Read */ std::chrono::duration_cast<std::chrono::milliseconds>( explode - read ).count(),
               /* Explode */ std::chrono::duration_cast<std::chrono::milliseconds>( tokenize - explode ).count(),
               /* Tokenize */ std::chrono::duration_cast<std::chrono::milliseconds>( preprocess - tokenize ).count(),
               /* Preprocess */ std::chrono::duration_cast<std::chrono::milliseconds>( parse - preprocess ).count(),
               /* Parse */ std::chrono::duration_cast<std::chrono::milliseconds>( end - parse ).count(),
               /* Total */ std::chrono::duration_cast<std::chrono::milliseconds>( end - read ).count()
               );

        if( !resultPP || !resultP )
        {
            for( const auto& line : parser.Log.Cached )
            {
                // if( line.second.front() == '<' )
                //    continue;

                DEBUG( filename.c_str(), "%s%s", std::string( line.first * 2, ' ' ).c_str(), line.second.c_str() );
            }
        }
    }
    #endif

    Status.Process.Files++;
    Status.Current.Clear();

    Status.Current.File = filename;

    bool                 updateFile = false, conflict = false, restart = false;
    std::string          content, newline = "\r\n";
    unsigned int         changes = 0;
    unsigned short       restartCount = 0;
    const unsigned short restartLimit = 1000;

    SStatus::SCurrent    previous;

    ScriptFile*          file = new ScriptFile();

    std::smatch          match;
    std::regex           define( "^[\\t\\ ]*\\#define[\\t\\ ]+([A-Za-z0-9_]+)(?:$|[\\t\\ ]+.*$)" );

    for( auto& line : lines )
    {
        // update status
        Status.Process.Lines++;
        Status.Current.Line = line;
        Status.Current.LineNumber++;

        // skip empty
        // lines containing only spaces and/or tabs will be cleared
        if( line.empty() || TextIsBlank( line ) )
        {
            content += newline;
            continue;
        }

        line.erase( line.find_last_not_of( "\t " ) + 1 );

        // skip fully commented
        if( TextIsComment( line ) )
        {
            content += line + newline;
            continue;
        }
        else if( line.find( "//ReDefine::IgnoreLine//" ) != std::string::npos || line.find( "/*ReDefine::IgnoreLine*/" ) != std::string::npos )
        {
            // DEBUG( nullptr, "SKIP" );
            content += line + newline;
            continue;
        }

        // detect merge conflicts
        if( TextIsConflict( line ) )
        {
            WARNING( nullptr, "possible merge conflict" );
            Status.Process.Counters["!Possible merge conflicts!"][Status.Current.File]++;
            conflict = true;
        }

        std::string defineName, defineValue;
        if( std::regex_match( line, match, define ) )
        {
            file->Defines.push_back( match.str( 1 ) );
            // DEBUG( __FUNCTION__, "DEFINE [%s]", file->Defines.back().c_str() );
        }

        // save original line
        const std::string lineOld = line;

        restart = true;
        restartCount = 0;

        while( restart )
        {
            restart = false;
            if( restartCount > restartLimit )
            {
                WARNING( __FUNCTION__, "line processing stopped : reached restart limit<%u>", restartLimit );
                break;
            }

            // extract more or less interesting code
            std::vector<ScriptCode> extracted;
            TextGetVariables( line, extracted );
            TextGetFunctions( line, extracted );

            for( const ScriptCode& codeOld : extracted )
            {
                ScriptCode code = codeOld;
                code.Parent = this;
                code.File = file;

                // make sure type flag is set
                if( !code.IsVariable( nullptr ) && !code.IsFunction( nullptr ) )
                {
                    WARNING( __FUNCTION__, "script code<%s> ignored : type missing", code.Name.c_str() );
                    continue;
                }

                // prepare code to reflect configuration (required by some edit actions)
                if( code.IsVariable( nullptr ) )
                {
                    auto it = VariablesPrototypes.find( code.Name );
                    if( it != VariablesPrototypes.end() )
                    {
                        code.ReturnType = it->second;
                    }
                    else
                    {
                        code.ReturnType = "?";
                    }
                }
                else if( code.IsFunction( nullptr ) )
                {
                    auto it = FunctionsPrototypes.find( code.Name );
                    if( it != FunctionsPrototypes.end() )
                    {
                        code.ReturnType = it->second.ReturnType;
                        code.ArgumentsTypes = it->second.ArgumentsTypes;
                    }
                    else
                    {
                        code.ReturnType = "?";

                        if( !code.Arguments.empty() )
                        {
                            code.ArgumentsTypes.resize( code.Arguments.size() );
                            std::fill( code.ArgumentsTypes.begin(), code.ArgumentsTypes.end(), "?" );
                        }
                    }
                }

                code.Change( "script code", code.GetFullString() );

                // "preprocess"
                code.SetFlag( SCRIPT_CODE_BEFORE );
                ProcessScriptEdit( EditBefore, code );
                code.UnsetFlag( SCRIPT_CODE_BEFORE );

                // "process"
                ProcessScriptReplacements( code );

                // "postprocess"
                code.SetFlag( SCRIPT_CODE_AFTER );
                ProcessScriptEdit( EditAfter, code );
                code.UnsetFlag( SCRIPT_CODE_AFTER );

                if( code.Changes.size() >= 2 )
                    code.ChangeLog();

                // update if needed
                code.SetFullString();
                if( ScriptFormattingForced || TextGetPacked( codeOld.Full ) != TextGetPacked( code.Full ) )
                    line = TextGetReplaced( line, codeOld.Full, code.Full );

                // handle restart
                if( code.IsFlag( SCRIPT_CODE_RESTART ) )
                {
                    code.UnsetFlag( SCRIPT_CODE_RESTART );
                    restartCount++;
                    restart = true;
                }
            }
        }

        // process raw replacement
        ProcessRaw( line );

        // detect line change, ignore meaningless changes
        bool change = TextGetPacked( line ) != TextGetPacked( lineOld );
        if( !change )
            change = ScriptFormattingForced && line != lineOld;

        if( change )
        {
            // log changes
            // requires messing with Status.Current so log functions won't add unwanted info
            previous = Status.Current;
            Status.Current.Line.clear();
            ILOG( "@@" );

            Status.Current.Clear();
            // DEBUG( nullptr, "<- %s", TextGetPacked( lineOld ).c_str() );
            // DEBUG( nullptr, "-> %s", TextGetPacked( line ).c_str() );

            LOG( "<- %s", lineOld.c_str() );
            LOG( "-> %s", line.c_str() );

            Status.Current = previous;

            // update file status
            changes++;
            updateFile = true;
        }

        // we did it, Rotators!
        content += line + newline;
    }

    // never update file with merge conflict
    if( conflict && updateFile && !readOnly )
    {
        Status.Current.Line.clear();
        Status.Current.LineNumber = 0;

        WARNING( nullptr, "possible merge conflict : ignored all line changes<%u>", changes );
        updateFile = false;
    }

    Status.Current.Clear();
    delete file;

    // update changes counter
    if( updateFile )
    {
        Status.Process.FilesChanges++;
        Status.Process.LinesChanges += changes;
    }

    if( readOnly )
        return;

    if( updateFile )
    {
        std::size_t pos = content.find_last_not_of( newline + "\t " );
        if( pos != std::string::npos )
        {
            content.erase( ++pos );
            content += newline;
        }

        std::ofstream file;
        file.open( TextGetFilename( path, filename ), std::ios::out | std::ios::binary );

        if( file.is_open() )
        {
            file << content;
            file.close();
        }
        else
        {
            WARNING( __FUNCTION__, "cannot write file<%s>", TextGetFilename( path, filename ).c_str() );

            // revert changes counter
            Status.Process.FilesChanges--;
            Status.Process.LinesChanges -= changes;
        }
    }
}

void ReDefine::ProcessScriptReplacements( ScriptCode& code, bool refresh /* = false */ )
{
    std::string before, after;

    if( DebugChanges )
        before = code.GetFullString();

    if( refresh )
    {
        if( code.IsVariable( nullptr ) )
        {
            auto it = VariablesPrototypes.find( code.Name );
            if( it != VariablesPrototypes.end() )
                code.ReturnType = it->second;
        }
        else if( code.IsFunction( nullptr ) )
        {
            auto it = FunctionsPrototypes.find( code.Name );
            if( it != FunctionsPrototypes.end() )
            {
                code.ReturnType = it->second.ReturnType;
                code.ArgumentsTypes = it->second.ArgumentsTypes;
            }
        }
    }

    if( code.IsFunction( nullptr ) )
    {
        ProcessFunctionArguments( code );

        if( DebugChanges )
        {
            after = code.GetFullString();

            if( TextGetPacked( before ) != TextGetPacked( after ) )
                code.Change( "script replacement<function arguments>" + std::string( refresh ? " refresh" : "" ), after );
        }
    }

    if( code.IsVariable( nullptr ) || code.IsFunction( nullptr ) )
    {
        std::string replacement;

        if( DebugChanges )
            before = code.GetFullString();

        if( !IsMysteryDefineType( code.ReturnType ) )
        {
            ProcessOperator( code.ReturnType, code );

            if( DebugChanges )
                replacement = "operator";
        }
        // try to guess define name for right part
        else if( code.ReturnType == "?" && code.Operator.length() && code.OperatorArgument.length() )
        {
            ProcessValueGuessing( code.OperatorArgument );

            if( DebugChanges )
                replacement = "guessing";
        }

        if( DebugChanges )
        {
            after = code.GetFullString();

            if( TextGetPacked( before ) != TextGetPacked( after ) )
                code.Change( "script replacement<" + replacement + ">" + (refresh ? " refresh" : ""), after );
        }
    }
}

void ReDefine::ProcessScriptEdit( const std::map<unsigned int, std::vector<ReDefine::ScriptEdit>>& edits, ReDefine::ScriptCode& codeOld )
{
    // editing must always works on backup to prevent massive screwup,
    // original code will be updated only if there's no problems with *any* condition/result function
    // that, plus (intentional) massive spam in warning log should be enough to get user's attention (yeah, i don't belive that either... :P)
    ScriptCode        code = codeOld;
    const std::string timing = code.IsFlag( SCRIPT_CODE_BEFORE ) ? "Before" : code.IsFlag( SCRIPT_CODE_AFTER ) ? "After" : "";

    for( const auto& it : edits )
    {
        for( const ScriptEdit& edit : it.second )
        {
            const bool        debug = edit.Debug ? edit.Debug : DebugChanges;

            bool              run = false, first = true;
            const std::string change = "script edit<" +  timing + ":" + std::to_string( (long long)it.first ) + ":" + edit.Name + ">";
            std::string       before, after, log;

            // all conditions needs to be satisfied
            for( const ScriptEdit::Action& condition: edit.Conditions )
            {
                run = code.CallEditIf( condition.Name, condition.Values );

                if( debug && ( (first && run) || !first ) )
                    code.Change( change + " " + condition.Name + (condition.Values.size() ? (":" + TextGetJoined( condition.Values, "," ) ) : ""), run ? "true" : "false" );

                if( !run )
                    break;

                first = false;
            }

            if( !run )
                continue;

            first = true;

            // you are Result, you must Do
            for( const ScriptEdit::Action& result : edit.Results )
            {
                if( debug )
                    log = " " + result.Name + (result.Values.size() ? (":" + TextGetJoined( result.Values, "," ) ) : "");

                run = code.CallEditDo( result.Name, result.Values );

                if( !run )
                {
                    if( debug )
                        code.Change( change + log, "(ERROR)" );

                    WARNING( nullptr, "script edit<%s> aborted : result<%s> failed", edit.Name.c_str(), result.Name.c_str() );

                    return;
                }


                if( debug )
                    code.Change( change + log, code.GetFullString() );

                code.SetFlag( SCRIPT_CODE_EDITED );

                // handle restart
                if( code.IsFlag( SCRIPT_CODE_RESTART ) )
                {
                    // handle restart+refresh
                    if( code.IsFlag( SCRIPT_CODE_REFRESH ) )
                    {
                        code.UnsetFlag( SCRIPT_CODE_REFRESH );
                        ProcessScriptReplacements( code, true );
                    }

                    // push changes
                    codeOld = code;

                    return;
                }
            }     // for( const ScriptEdit::Action& result : edit.Results )

            // handle refresh
            if( code.IsFlag( SCRIPT_CODE_REFRESH ) )
            {
                code.UnsetFlag( SCRIPT_CODE_REFRESH );
                ProcessScriptReplacements( code, true );
            }
        }     // for( const ScriptEdit& edit : it.second )
    }         // for( const auto& it : edits )

    // push changes
    codeOld = code;
}
