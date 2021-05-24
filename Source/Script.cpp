#include <chrono>
#include <filesystem>
#include <fstream>

#include "Ini.h"

#include "ReDefine.h"

#if defined (HAVE_PARSER)
# include "Parser.h"
#endif

ReDefine::ScriptEdit::External ReDefine::ScriptEdit::ExternalDummy;

//

ReDefine::ScriptEdit::ScriptEdit() :
    Debug( false ),
    Name( "" )
{}

//

ReDefine::ScriptEdit::External::External() :
    Name( "" ),
    RunConditions( false ),
    RunResults( false ),
    ReturnConditions( ScriptEditReturn::Invalid ),
    ReturnResults( ScriptEditReturn::Invalid )
{}

ReDefine::ScriptEdit::External::External( const std::string& name, bool conditions, bool results ) :
    Name( name ),
    RunConditions( conditions ),
    RunResults( results ),
    ReturnConditions( ScriptEditReturn::Invalid ),
    ReturnResults( ScriptEditReturn::Invalid )
{}

bool ReDefine::ScriptEdit::External::InUse()
{
    return !Name.empty() && (RunConditions || RunResults);
}

//

ReDefine::ScriptEditAction::ScriptEditAction( void* root, const ScriptEdit::Action& action, ScriptEditAction::Flag& flags ) :
    Name( action.Name ),
    Values( action.Values ),
    Root( static_cast<ReDefine*>(root) ),
    Flags( flags )
{}

typedef std::underlying_type<ReDefine::ScriptEditAction::Flag>::type ScriptEditActionFlagType;

bool ReDefine::ScriptEditAction::IsFlag( ScriptEditAction::Flag flag ) const
{
    return (static_cast<ScriptEditActionFlagType>(Flags) & static_cast<ScriptEditActionFlagType>(flag) ) != 0;
}

void ReDefine::ScriptEditAction::SetFlag( ScriptEditAction::Flag flag )
{
    Flags = static_cast<ScriptEditAction::Flag>(static_cast<ScriptEditActionFlagType>(Flags) | static_cast<ScriptEditActionFlagType>(flag) );
}

void ReDefine::ScriptEditAction::UnsetFlag( ScriptEditAction::Flag flag )
{
    Flags = static_cast<ScriptEditAction::Flag>( (static_cast<ScriptEditActionFlagType>(Flags) | static_cast<ScriptEditActionFlagType>(flag) ) ^ static_cast<ScriptEditActionFlagType>(flag) );
}

bool ReDefine::ScriptEditAction::IsBefore( const char* caller ) const
{
    if( !IsFlag( ScriptEditAction::Flag::BEFORE ) )
    {
        if( caller )
            Root->WARNING( caller, "action can be used only in RunBefore edits" );

        return false;
    }

    return true;
}

bool ReDefine::ScriptEditAction::IsAfter( const char* caller ) const
{
    if( !IsFlag( ScriptEditAction::Flag::AFTER ) )
    {
        if( caller )
            Root->WARNING( caller, "action can be used only in RunAfter edits" );

        return false;
    }

    return true;
}

bool ReDefine::ScriptEditAction::IsOnDemand( const char* caller ) const
{
    if( !IsFlag( ScriptEditAction::Flag::DEMAND ) )
    {
        if( caller )
            Root->WARNING( caller, "action can be used only in RunOnDemand edits" );

        return false;
    }

    return true;
}

bool ReDefine::ScriptEditAction::IsValues( const char* caller, const unsigned int& count ) const
{
    if( Values.size() < count )
    {
        if( caller )
            Root->WARNING( caller, "wrong number of arguments : required<%u> provided<%u>", count, Values.size() );

        return false;
    }

    for( unsigned int c = 0; c < count; c++ )
    {
        if( Values[c].empty() )
        {
            if( caller )
                Root->WARNING( caller, "wrong number of arguments : argument<%u> is empty", c + 1 );

            return false;
        }
    }

    return true;
}

bool ReDefine::ScriptEditAction::GetINDEX( const char* caller, const unsigned int& val, const ScriptCode& code, unsigned int& out ) const
{
    unsigned int tmp;
    if( !GetUINT( caller, val, tmp, "INDEX" ) )
        return false;

    if( tmp >= code.Arguments.size() )
    {
        if( caller )
            Root->WARNING( caller, "INDEX<%u> out of range", tmp );

        return false;
    }

    out = tmp;
    return true;
}

bool ReDefine::ScriptEditAction::GetTYPE( const char* caller, const unsigned int& val, bool allowUnknown /* = false */ ) const
{
    if( val >= Values.size() )
    {
        if( caller )
            Root->WARNING( caller, "wrong number of arguments : required<%u> provided<%u>", val, Values.size() ); // TODO bit misleading

        return false;
    }

    if( Values[val].empty() )
    {
        if( caller )
            Root->WARNING( caller, "TYPE is empty" );

        return false;
    }

    if( allowUnknown && Root->IsMysteryDefineType( Values[val] ) )
        return true;

    if( !Root->IsDefineType( Values[val] ) )
    {
        if( caller )
            Root->WARNING( caller, "unknown TYPE<%s>", Values[val].c_str() );

        return false;
    }

    return true;
}

bool ReDefine::ScriptEditAction::GetUINT( const char* caller, const unsigned int& val, unsigned int& out, const std::string& name /* = "UINT" */ ) const
{
    if( val >= Values.size() )
    {
        if( caller )
            Root->WARNING( caller, "wrong number of arguments : required<%u> provided<%u>", val, Values.size() ); // TODO bit misleading

        return false;
    }

    int tmp = -1;
    if( !Root->TextIsInt( Values[val] ) || !Root->TextGetInt( Values[val], tmp ) )
    {
        if( caller )
            Root->WARNING( caller, "invalid %s<%s>", name.c_str(), Values[val].c_str() );

        return false;
    }

    if( tmp < 0 )
    {
        if( caller )
            Root->WARNING( caller, "invalid %s<%s> : value < 0", name.c_str(), Values[val].c_str() );

        return false;
    }

    out = tmp;
    return true;
}

// checks if given edit action exists before trying to call it
// as EditIf/EditDo should be free to modify by main application at any point,
// it can use EditIf["IfThing"](...) like there's no tomorrow, but ReDefine class doing same thing is Bad Idea (TM)

ReDefine::ScriptEditReturn ReDefine::ScriptEditAction::CallEditIf( const ScriptCode& code )
{
    auto it = Root->EditIf.find( Name );
    if( it == Root->EditIf.end() )
    {
        Root->WARNING( __FUNCTION__, "script action condition<%s> not found", Name.c_str() );
        return ScriptEditReturn::Invalid;
    }

    return it->second( *this, code );
}

ReDefine::ScriptEditReturn ReDefine::ScriptEditAction::CallEditIf( const ScriptCode& code, const std::string& name, std::vector<std::string> values /* = std::vector<std::string>() */ )
{
    ScriptEdit::Action action;
    action.Name = name;
    action.Values = values;

    ScriptEditAction data( Root, action, Flags );

    return data.CallEditIf( code );
}

ReDefine::ScriptEditReturn ReDefine::ScriptEditAction::CallEditDo( ScriptCode& code )
{
    auto it = Root->EditDo.find( Name );
    if( it == Root->EditDo.end() )
    {
        Root->WARNING( __FUNCTION__, "script action result<%s> not found", Name.c_str() );
        return ScriptEditReturn::Invalid;
    }

    return it->second( *this, code );
}

ReDefine::ScriptEditReturn ReDefine::ScriptEditAction::CallEditDo( ScriptCode& code, const std::string& name, std::vector<std::string> values /* = std::vector<std::string>() */ )
{
    ScriptEdit::Action action;
    action.Name = name;
    action.Values = values;

    ScriptEditAction data( Root, action, Flags );

    return data.CallEditDo( code );
}

//

ReDefine::ScriptCode::ScriptCode( const ScriptCode::Flag& flags /* = ScriptCode::Flag::NONE */ ) :
    Parent( nullptr ),
    File( nullptr ),
    Flags( flags ),
    Full( "" ),
    Name( "" ),
    ReturnType( "" ),
//  Arguments(),
    Operator( "" ),
    OperatorArgument( "" )
{}

typedef std::underlying_type<ReDefine::ScriptCode::Flag>::type ScriptCodeFlagType;

bool ReDefine::ScriptCode::IsFlag( ScriptCode::Flag flag ) const
{
    return (static_cast<ScriptCodeFlagType>(Flags) & static_cast<ScriptCodeFlagType>(flag) ) != 0;
}

void ReDefine::ScriptCode::SetFlag( ScriptCode::Flag flag )
{
    Flags = static_cast<ScriptCode::Flag>(static_cast<ScriptCodeFlagType>(Flags) | static_cast<ScriptCodeFlagType>(flag) );
}

void ReDefine::ScriptCode::UnsetFlag( ScriptCode::Flag flag )
{
    Flags = static_cast<ScriptCode::Flag>( (static_cast<ScriptCodeFlagType>(Flags) | static_cast<ScriptCodeFlagType>(flag) ) ^ static_cast<ScriptCodeFlagType>(flag) );
}

void ReDefine::ScriptCode::SetType( const ReDefine::ScriptCode::Flag& type )
{
    UnsetFlag( ScriptCode::Flag::VARIABLE );
    UnsetFlag( ScriptCode::Flag::FUNCTION );

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
                std::vector<std::string> args, raw;

                for( auto& arg : Arguments )
                {
                    args.push_back( arg.Arg );

                    if( Parent->ScriptFormatting == ScriptCode::Format::UNCHANGED )
                        raw.push_back( arg.Raw );
                }

                switch( Parent->ScriptFormatting )
                {
                    case ScriptCode::Format::UNCHANGED:
                        result += Parent->TextGetJoined( raw, "," );
                        break;
                    case ScriptCode::Format::WIDE:
                        result += " " + Parent->TextGetJoined( args, ", " ) + " ";
                        break;
                    case ScriptCode::Format::MEDIUM:
                        result += Parent->TextGetJoined( args, ", " );
                        break;
                    case ScriptCode::Format::TIGHT:
                        result += Parent->TextGetJoined( args, "," );
                        break;
                    default:
                        Parent->WARNING( __FUNCTION__, "unknown formatting<%u> : switching to default<%u>", Parent->ScriptFormatting, ScriptCode::Format::DEFAULT );
                        Parent->ScriptFormatting = ScriptCode::Format::DEFAULT;
                        result += Parent->TextGetJoined( args, ", " );
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

void ReDefine::ScriptCode::SetFunctionArgumentsTypes( const ReDefine::FunctionProto& proto )
{
    if( !IsFunction( __FUNCTION__ ) )
        return;

    if( Arguments.size() != proto.ArgumentsTypes.size() )
        Parent->WARNING( __FUNCTION__, "invalid number of function<%s> arguments : expected<%u> found<%u>", Name.c_str(), proto.ArgumentsTypes.size(), Arguments.size() );

    auto it = proto.ArgumentsTypes.begin();
    auto end = proto.ArgumentsTypes.end();

    for( Argument& argument : Arguments )
    {
        if( it != end )
            argument.Type = *it;
        else
            argument.Type = "?";

        ++it;
    }
}

//

bool ReDefine::ScriptCode::IsVariable( const char* caller ) const
{
    if( Name.empty() )
    {
        if( caller )
            Parent->WARNING( caller, "name not set" );

        return false;
    }

    if( !IsFlag( ScriptCode::Flag::VARIABLE ) )
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

    if( !IsFlag( ScriptCode::Flag::FUNCTION ) )
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

    if( !IsFlag( ScriptCode::Flag::VARIABLE ) && !IsFlag( ScriptCode::Flag::FUNCTION ) )
    {
        if( caller )
            Parent->WARNING( caller, "script code<%s> is not a variable or function", Name.c_str() );

        return false;
    }

    return true;
}

//


void ReDefine::ScriptCode::Change( const std::string& left, const std::string& right /* = std::string() */ )
{
    Changes.emplace_back( left, right );
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

// script edit helpers

static ReDefine::ScriptEditReturn RunExternal( const char* caller, ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code, bool condition )
{
    if( !code.IsFunction( caller ) )
        return action.Invalid();

    if( !action.IsValues( caller, 3 ) )
        return action.Invalid();

    unsigned int idx = unsigned int(-1);

    if( !action.GetINDEX( caller, 0, code, idx ) )
        return action.Invalid();

    if( action.Values[1].empty() || action.Values[2].empty() )
    {
        action.Root->DEBUG( caller, "External condition/result name not set" );
        return action.Invalid();
    }

    ReDefine::ScriptCode              codeExtracted = code;

    std::vector<ReDefine::ScriptCode> extracted;
    action.Root->TextGetFunctions( code.Arguments[idx].Raw, extracted );
    action.Root->TextGetVariables( code.Arguments[idx].Raw, extracted );

    if( extracted.empty() )
    {
        // TODO retry with ^[A-Za-z0-9_]$
        // action.Root->DEBUG( caller, "Extracting argument<%u> failed", idx );
        return action.Failure();
    }

    extracted.front().Parent = code.Parent;
    extracted.front().File = code.File;

    if( extracted.size() != 1 )
        action.Root->DEBUG( caller, "Extracted argument : ScriptCode size = %u, using first only = %s", extracted.size(), extracted.front().GetFullString().c_str() );

    codeExtracted = extracted.front();

    bool                           restart = false;
    ReDefine::ScriptEdit::External external( action.Values[1] + "->" + action.Values[2], condition, !condition );

    code.Changes.push_back( std::make_pair<std::string, std::string>( "script code (extracted)", codeExtracted.GetFullString() ) );
    action.Root->ProcessScriptEdit( ReDefine::ScriptEditAction::Flag::DEMAND, action.Root->EditOnDemand, codeExtracted, restart, external );

    for( const auto& change : codeExtracted.Changes )
    {
        code.Changes.push_back( change );
    }

    if( condition )
    {
        /*
           if(action.Root->TextGetPacked(code.GetFullString()) != action.Root->TextGetPacked(codeExtracted.GetFullString()))
           {
            // TODO? unify condition/result functions signatures and validate by ProcessEditScript()
            action.Root->WARNING(caller, "External condition attepted to modify script code, cannot continue");
            action.Root->WARNING(caller, "<%s> != <%s>", action.Root->TextGetPacked(code.GetFullString()).c_str(), action.Root->TextGetPacked(codeExtracted.GetFullString()).c_str());
                return action.Invalid();
           }
         */

        if( restart )
        {
            action.Root->WARNING( caller, "ProcessScriptEdit() unexpected restart=true, cannot continue" );
            return action.Invalid();
        }
    }

    if( condition && external.ReturnConditions != ReDefine::ScriptEditReturn::Success )
        return external.ReturnConditions;
    else if( !condition && external.ReturnResults != ReDefine::ScriptEditReturn::Success )
        return external.ReturnResults;

    if( !condition )
    {
        code.Arguments[idx].Raw = codeExtracted.Full;
        code.Arguments[idx].Arg = codeExtracted.GetFullString();

        if( codeExtracted.IsFlag( ReDefine::ScriptCode::Flag::REFRESH ) )
            code.SetFlag( ReDefine::ScriptCode::Flag::REFRESH );

        if( restart )
            action.SetFlag( ReDefine::ScriptEditAction::Flag::RESTART );
    }

    if( condition )
        return external.ReturnConditions;
    else
        return external.ReturnResults;
}

// script edit conditions

// ? IfArgumentCondition:INDEX,STRING,STRING

static ReDefine::ScriptEditReturn IfArgumentCondition( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    return RunExternal( __FUNCTION__, action, const_cast<ReDefine::ScriptCode&>(code), true );
}

// ? IfArgumentIs:INDEX,DEFINE
// ? IfArgumentIs:INDEX,DEFINE,TYPE
// ! Checks if script function argument at INDEX resolves to DEFINE value.
static ReDefine::ScriptEditReturn IfArgumentIs( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsFunctionKnown( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 2 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    if( action.Root->TextIsInt( code.Arguments[idx].Arg ) )
    {
        int         val = -1;
        std::string type = code.Arguments[idx].Type, value;

        if( action.IsValues( nullptr, 3 ) )
        {
            if( action.GetTYPE( __FUNCTION__, 2 ) )
                type = action.Values[2];
            else
                return action.Invalid();
        }

        if( !action.Root->TextGetInt( code.Arguments[idx].Arg, val ) || !action.Root->GetDefineName( type, val, value ) )
            return action.Failure();

        // action.Root->DEBUG( __FUNCTION__, "parsed compare: %s == %s", value.c_str(), values[1].c_str() );
        return action.Return( value == action.Values[1] );
    }

    // action.Root->DEBUG( __FUNCTION__, "raw compare: %s == %s", code.Arguments[idx].c_str(), values[1].c_str() );
    return action.Return( code.Arguments[idx].Arg == action.Values[1] );
}

// ? IfArgumentValue:INDEX,STRING
// ! Checks if script function argument at INDEX equals STRING.
static ReDefine::ScriptEditReturn IfArgumentValue( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 2 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    return action.Return( code.Arguments[idx].Arg == action.Values[1] );
}

// DISABLED IfArgumentNotValue:INDEX,STRING
// ! Checks if script function argument at INDEX does not equals STRING.
// static bool IfArgumentNotValue( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
// {
//     if( !code.IsFunction( __FUNCTION__ ) )
//         return false;
//
//     if( !code.IsValues( __FUNCTION__, values, 2 ) )
//         return false;
//
//     unsigned int idx;
//     if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
//         return false;
//
//     return code.Arguments[idx].Arg != values[1];
// }

// ? IfArgumentsEqual:INDEX,INDEX
// ! Checks if script function arguments at INDEXes are equal.
static ReDefine::ScriptEditReturn IfArgumentsEqual( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 2 ) )
        return action.Invalid();

    unsigned int idx1;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx1 ) )
        return action.Invalid();

    unsigned int idx2;
    if( !action.GetINDEX( __FUNCTION__, 1, code, idx2 ) )
        return action.Invalid();

    if( idx1 == idx2 )
    {
        action.Root->WARNING( __FUNCTION__, "duplicated INDEX<%u>", idx1 );
        return action.Invalid();
    }

    return action.Return( code.Arguments[idx1].Arg == code.Arguments[idx2].Arg );
}

// ? IfArgumentsSize:UINT
// ! Checks if script function have UINT arguments.
static ReDefine::ScriptEditReturn IfArgumentsSize( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    unsigned int size;
    if( !action.GetUINT( __FUNCTION__, 0, size ) )
        return action.Invalid();

    return action.Return( code.Arguments.size() == size );
}

// ? IfEdited
// ! Checks if script code has been processed by any action result.
static ReDefine::ScriptEditReturn IfEdited( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    return action.Return( code.IsFlag( ReDefine::ScriptCode::Flag::EDITED ) );
}

// DISABLED IfFileDefined:STRING
// static bool IfFileDefined( const ReDefine::ScriptCode& code, const std::vector<std::string>& values )
// {
//    if( !code.IsValues( __FUNCTION__, values, 1 ) )
//        return false;
//
//    return std::find( code.File->Defines.begin(), code.File->Defines.end(), values[0] ) != code.File->Defines.end();
// }

// ? IfFileName:STRING
static ReDefine::ScriptEditReturn IfFileName( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& /* code */ )
{
    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    return action.Return( action.Root->Status.Current.File == action.Values[0] );
}

// ? IfFunction
// ? IfFunction:STRING
// ? IfFunction:STRING_1,...,STRING_N
// ! Checks if script code is a function.
// ! When used with argument(s), checks if script code is a function with given name. One or more names can be passed to simulate `or` / `||` check.
// > IfName
static ReDefine::ScriptEditReturn IfFunction( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( nullptr ) )
        return action.Failure();

    if( action.IsValues( nullptr, 1 ) )
    {
        for( const auto& value : action.Values )
        {
            if( value.empty() )
                break;

            if( action.CallEditIf( code, "IfName", { value } ) == ReDefine::ScriptEditReturn::Success )
                return action.Success();
        }

        return action.Failure();
    }

    return action.Success();
}

// ? IfName:STRING
// ! Checks if script function/variable name equals STRING.
static ReDefine::ScriptEditReturn IfName( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    return action.Return( code.Name == action.Values[0] );
}

// DISABLED IfNotEdited
// static bool IfNotEdited( const ReDefine::ScriptCode& code, const std::vector<std::string>& /* values */ )
// {
//     return !code.IsFlag( ReDefine::ScriptCode::Flag::EDITED );
// }

// ? IfOperator
static ReDefine::ScriptEditReturn IfOperator( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    return action.Return( code.Operator.length() > 0 );
}

// ? IfOperatorName:STRING
static ReDefine::ScriptEditReturn IfOperatorName( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    if( code.Operator.empty() )
        return action.Failure();

    return action.Return( action.Root->GetOperatorName( code.Operator ) == action.Values[0] );
}

// ? IfOperatorValue:STRING
static ReDefine::ScriptEditReturn IfOperatorValue( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    if( code.OperatorArgument.empty() )
        return action.Failure();

    return action.Return( code.OperatorArgument == action.Values[0] );
}

// ? IfReturnType:TYPE
static ReDefine::ScriptEditReturn IfReturnType( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    if( !action.GetTYPE( __FUNCTION__, 0, true ) )
        return action.Invalid();

    return action.Return( code.ReturnType == action.Values[0] );
}

// ? IfVariable
// ? IfVariable:STRING
// ? IfVariable:STRING_1,...,STRING_N
// ! Checks if script code is a variable. One or more names can be passed to simulate `or` / `||` check.
// > IfName
static ReDefine::ScriptEditReturn IfVariable( ReDefine::ScriptEditAction& action, const ReDefine::ScriptCode& code )
{
    if( !code.IsVariable( nullptr ) )
        return action.Failure();

    if( action.IsValues( nullptr, 1 ) )
    {
        for( const auto& value : action.Values )
        {
            if( action.CallEditIf( code, "IfName", { value } ) == ReDefine::ScriptEditReturn::Success )
                return action.Success();
        }

        return action.Failure();
    }

    return action.Success();
}

// script edit results

// ? DoArgumentCount:INDEX,STRING
static ReDefine::ScriptEditReturn DoArgumentCount( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 2 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    action.Root->Status.Process.Counters[action.Values[1]][code.Arguments[idx].Arg]++;

    return action.Success();
}

// ? DoArgumentResult:INDEX,STRING,STRING

static ReDefine::ScriptEditReturn DoArgumentResult( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    return RunExternal( __FUNCTION__, action, code, false );
}

// ? DoArgumentSet:INDEX,STRING
static ReDefine::ScriptEditReturn DoArgumentSet( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 2 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    code.Arguments[idx].Raw = action.Root->TextGetReplaced( code.Arguments[idx].Raw, code.Arguments[idx].Arg, action.Values[1] );
    code.Arguments[idx].Arg = action.Values[1];

    return action.Success();
}

// ? DoArgumentSetPrefix:INDEX,STRING
// > DoArgumentSet
static ReDefine::ScriptEditReturn DoArgumentSetPrefix( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 2 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    return action.CallEditDo( code, "DoArgumentSet", { action.Values[0], action.Values[1] + code.Arguments[idx].Arg } );
}

// ? DoArgumentSetSuffix:INDEX,STRING
// > DoArgumentSet
static ReDefine::ScriptEditReturn DoArgumentSetSuffix( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 2 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    return action.CallEditDo( code, "DoArgumentSet", { action.Values[0], code.Arguments[idx].Arg + action.Values[1] } );
}

// ? DoArgumentSetType:INDEX,TYPE
static ReDefine::ScriptEditReturn DoArgumentSetType( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !action.IsBefore( __FUNCTION__ ) )
        return action.Invalid();

    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 2 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    code.Arguments[idx].Type = action.Values[1];

    return action.Success();
}

// ? DoArgumentLookup:INDEX
// ? DoArgumentLookup:INDEX,STRING
static ReDefine::ScriptEditReturn DoArgumentLookup( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    bool        unknown = false;
    std::string counter;

    if( action.IsValues( nullptr, 2 ) )
    {
        if( action.Values[1] == "!UNKNOWN!" )
            unknown = true;
        else
            counter = action.Values[1];
    }

    if( action.Root->TextIsInt( code.Arguments[idx].Arg ) )
    {
        int         val = -1;
        std::string value;

        if( !action.Root->TextGetInt( code.Arguments[idx].Arg, val ) )
        {
            action.Root->WARNING( __FUNCTION__, "cannot convert string<%s> into integer", code.Arguments[idx].Arg.c_str() );
            return action.Success(); // ???
        }

        if( !action.Root->GetDefineName( code.Arguments[idx].Arg, val, value ) )
        {
            action.Root->WARNING( __FUNCTION__, "unknown %s<%s>", code.Arguments[idx].Type.c_str(), code.Arguments[idx].Arg.c_str() );

            if( unknown )
                action.Root->Status.Process.Counters["!Unknown " + code.Arguments[idx].Type + "!"][code.Arguments[idx].Arg]++;
            else if( !counter.empty() )
                action.Root->Status.Process.Counters[counter][code.Arguments[idx].Arg]++;

            return action.Success();
        }
    }
    else
    {
        int val = -1;
        if( !action.Root->GetDefineValue( code.Arguments[idx].Type, code.Arguments[idx].Arg, val ) )
        {
            action.Root->WARNING( __FUNCTION__, "unknown %s<%s>", code.Arguments[idx].Type.c_str(), code.Arguments[idx].Arg.c_str() );

            if( unknown )
                action.Root->Status.Process.Counters["!Unknown " + code.Arguments[idx].Type + "!"][code.Arguments[idx].Arg]++;
            else if( !counter.empty() )
                action.Root->Status.Process.Counters[counter][code.Arguments[idx].Arg]++;

            return action.Success();
        }
    }

    return action.Success();
}

// ? DoArgumentsClear
// ! Removes all script function arguments.
static ReDefine::ScriptEditReturn DoArgumentsClear( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    code.Arguments.clear();

    return action.Success();
}

// ? DoArgumentsErase:INDEX
// ! Removes script function argument at INDEX.
static ReDefine::ScriptEditReturn DoArgumentsErase( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code  )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    // try to keep original formatting of 1st argument
    if( action.Root->ScriptFormatting == ReDefine::ScriptCode::Format::UNCHANGED && idx == 0 && code.Arguments.size() >= 2 )
        code.Arguments[1].Raw = action.Root->TextGetReplaced( code.Arguments.front().Raw, code.Arguments.front().Arg, code.Arguments[1].Arg );

    code.Arguments.erase( code.Arguments.begin() + idx );

    return action.Success();
}

// ? DoArgumentsMoveBack:INDEX
// ! Moves script function argument at INDEX to the end of list.
// > DoArgumentsErase
// > DoArgumentsPushBack
static ReDefine::ScriptEditReturn DoArgumentsMoveBack( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    std::string arg, type = code.Arguments[idx].Type;
    // try to keep original formatting of arguments
    if( action.Root->ScriptFormatting == ReDefine::ScriptCode::Format::UNCHANGED && code.Arguments.size() >= 2 )
        arg = action.Root->TextGetReplaced( code.Arguments.back().Raw, code.Arguments.back().Arg, code.Arguments[idx].Arg );
    else
        arg = code.Arguments[idx].Arg;

    if( action.CallEditDo( code, "DoArgumentsErase", { action.Values[0] } ) != ReDefine::ScriptEditReturn::Success )
        return action.Invalid();

    if( action.CallEditDo( code, "DoArgumentsPushBack", { arg, type } ) != ReDefine::ScriptEditReturn::Success )
        return action.Invalid();

    return action.Success();
}

// ? DoArgumentsMoveFront:INDEX
// ! Moves script function argument at INDEX to the start of list.
// > DoArgumentsErase
// > DoArgumentsPushFront
static ReDefine::ScriptEditReturn DoArgumentsMoveFront( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetINDEX( __FUNCTION__, 0, code, idx ) )
        return action.Invalid();

    std::string arg0, arg1, type = code.Arguments[idx].Type;
    // try to keep original formatting of arguments
    if( action.Root->ScriptFormatting == ReDefine::ScriptCode::Format::UNCHANGED && code.Arguments.size() >= 2 )
    {
        arg0 = action.Root->TextGetReplaced( code.Arguments.front().Raw, code.Arguments.front().Arg, code.Arguments[idx].Arg );
        arg1 = action.Root->TextGetReplaced( code.Arguments[idx].Raw, code.Arguments[idx].Arg, code.Arguments.front().Arg );
    }
    else
        arg0 = code.Arguments[idx].Arg;

    if( action.CallEditDo( code, "DoArgumentsErase", { action.Values[0] } ) != ReDefine::ScriptEditReturn::Success )
        return action.Invalid();

    if( action.CallEditDo( code, "DoArgumentsPushFront", { arg0, type } ) != ReDefine::ScriptEditReturn::Success )
        return action.Invalid();

    if( !arg1.empty() )
        code.Arguments[1].Raw = arg1;

    return action.Success();
}

// ? DoArgumentsPushBack:STRING
// ? DoArgumentsPushBack:STRING,TYPE
// ! Adds script function argument as last in list.
static ReDefine::ScriptEditReturn DoArgumentsPushBack( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    std::string type = "?";

    if( action.IsValues( nullptr, 2 ) )
    {
        if( !action.GetTYPE( __FUNCTION__, 1, true ) )
            return action.Invalid();

        type = action.Values[1];
    }

    ReDefine::ScriptCode::Argument argument;
    argument.Raw = action.Values[0];
    argument.Arg = action.Root->TextGetTrimmed( action.Values[0] );
    argument.Type = type;

    code.Arguments.push_back( argument );

    return action.Success();
}

// ? DoArgumentsPushFront:STRING,
// ? DoArgumentsPushFront:STRING,TYPE
// ! Adds script function argument as first in list.
static ReDefine::ScriptEditReturn DoArgumentsPushFront( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    std::string type = "?";

    if( action.IsValues( nullptr, 2 ) )
    {
        if( !action.GetTYPE( __FUNCTION__, 1, true ) )
            return action.Invalid();

        type = action.Values[1];
    }

    ReDefine::ScriptCode::Argument argument;
    argument.Raw = action.Values[0];
    argument.Arg = action.Root->TextGetTrimmed( action.Values[0] );
    argument.Type = type;
    code.Arguments.insert( code.Arguments.begin(), argument );

    return action.Success();
}

// ? DoArgumentsResize:UINT
// ! Changes script function arguments count to UINT.
// > DoArgumentsClear (if arguments count is 0)
static ReDefine::ScriptEditReturn DoArgumentsResize( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code  )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    unsigned int size;
    if( !action.GetUINT( __FUNCTION__, 0, size ) )
        return action.Invalid();

    if( !size )
        return action.CallEditDo( code, "DoArgumentsClear", {} );

    code.Arguments.resize( size );

    for( ReDefine::ScriptCode::Argument& argument : code.Arguments )
    {
        if( argument.Arg.empty() )
        {
            argument.Arg = argument.Raw = "/* not set */";
            argument.Type = "?";
        }
    }

    return action.Success();
}

// ? DoFileCount:STRING
static ReDefine::ScriptEditReturn DoFileCount( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& /* code */ )
{
    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    action.Root->Status.Process.Counters[action.Values[0]][action.Root->Status.Current.File]++;

    return action.Success();
}

// ? DoFunction
// ? DoFunction:STRING
// ! Converts script code into function.
// > DoNameSet
static ReDefine::ScriptEditReturn DoFunction( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    code.SetType( ReDefine::ScriptCode::Flag::FUNCTION );

    if( action.IsValues( nullptr, 1 ) )
    {
        if( action.CallEditDo( code, "DoNameSet", { action.Values[0] } ) != ReDefine::ScriptEditReturn::Success )
            return action.Invalid();
    }

    code.SetFlag( ReDefine::ScriptCode::Flag::REFRESH );

    return action.Success();
}

// ? DoFunctionAround:STRING
// ! Surrounds script code with function
// > DoArgumentsClear
// > DoArgumentsPushBack
// > DoFunction
// > DoOperatorClear
static ReDefine::ScriptEditReturn DoFunctionAround( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    bool        wasFunction = code.IsFlag( ReDefine::ScriptCode::Flag::FUNCTION );
    std::string full = code.GetFullString();

    action.CallEditDo( code, "DoFunction", { action.Values[0] } );

    if( wasFunction )
        action.CallEditDo( code, "DoArgumentsClear" );

    action.CallEditDo( code, "DoArgumentsPushBack", { full } );
    action.CallEditDo( code, "DoOperatorClear" );

    code.ReturnType = "?";

    return action.Success();
}

// ? DoFunctionAroundArgument:STRING,UINT
static ReDefine::ScriptEditReturn DoFunctionAroundArgument( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 2 ) )
        return action.Invalid();

    unsigned int idx;
    if( !action.GetUINT( __FUNCTION__, 1, idx ) )
        return action.Invalid();

    // there's no way to know what original formatting should be, when running with ScriptCode::Format::UNCHANGED;
    // best bet is to surround argument with f() preserving leading/trailing spaces
    code.Arguments[idx].Raw = action.Root->TextGetReplaced( code.Arguments[idx].Raw, code.Arguments[idx].Arg, action.Values[0] + "(" + code.Arguments[idx].Arg + ")" );
    code.Arguments[idx].Arg = action.Values[0] + "(" + code.Arguments[idx].Arg + ")";
    code.Arguments[idx].Type = "?"; // TODO? allow to set via action values

    return action.Success();
}

// ? DoLogCurrentLine
// ? DoLogCurrentLine:STRING
// ! Adds an entry with currently processed script function/variable to one of log files.
static ReDefine::ScriptEditReturn DoLogCurrentLine( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& /* code */ )
{
    ReDefine::SStatus::SCurrent previous = action.Root->Status.Current;
    action.Root->Status.Current.Line.clear();

    if( !action.Values.empty() && !action.Values[0].empty() )
    {
        if( action.Values[0] == "DEBUG" )
        {
            action.Root->DEBUG( nullptr, "%%%%" );
            action.Root->Status.Current.Clear();
            action.Root->DEBUG( nullptr, "   %s", previous.Line.c_str() );
        }
        else if( action.Values[0] == "WARNING" )
        {
            action.Root->WARNING( nullptr, "%%%%" );
            action.Root->Status.Current.Clear();
            action.Root->WARNING( nullptr, "   %s", previous.Line.c_str() );
        }
    }
    else
    {
        action.Root->ILOG( "%%%%" );
        action.Root->LOG( "   %s", previous.Line.c_str() );
    }

    action.Root->Status.Current = previous;

    return action.Success();
}

// ? DoNameCount:STRING
static ReDefine::ScriptEditReturn DoNameCount( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    action.Root->Status.Process.Counters[action.Values[0]][code.Name]++;

    return action.Success();
}

// ? DoNameSet:STRING
// ! Changes script function/variable name.
static ReDefine::ScriptEditReturn DoNameSet( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    code.Name = action.Values[0];
    code.SetFlag( ReDefine::ScriptCode::Flag::REFRESH );

    return action.Success();
}

// ? DoOperatorClear
// ! Removes script function/variable operator.
static ReDefine::ScriptEditReturn DoOperatorClear( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    code.Operator.clear();
    code.OperatorArgument.clear();

    return action.Success();
}

// ? DoOperatorSet:STRING
// ? DoOperatorSet:STRING,STRING
// ! Adds/changes script function/variable operator.
static ReDefine::ScriptEditReturn DoOperatorSet( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    code.Operator = action.Values[0];

    if( action.IsValues( nullptr, 2 ) )
        code.OperatorArgument = action.Values[1];

    return action.Success();
}

// ? DoRestart
// ! Restarts line processing after applying changes.
static ReDefine::ScriptEditReturn DoRestart( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& /* code */ )
{
    action.SetFlag( ReDefine::ScriptEditAction::Flag::RESTART );

    return action.Success();
}

// ? DoReturnSetType:TYPE
// ! Sets script function/variable return type.
static ReDefine::ScriptEditReturn DoReturnSetType( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( !action.IsBefore( __FUNCTION__ ) )
        return action.Invalid();

    if( !code.IsVariableOrFunction( __FUNCTION__ ) )
        return action.Invalid();

    if( !action.IsValues( __FUNCTION__, 1 ) )
        return action.Invalid();

    if( !action.GetTYPE( __FUNCTION__, 0 ) )
        return action.Invalid();

    code.ReturnType = action.Values[0];

    return action.Success();
}

// ? DoVariable
// ? DoVariable:STRING
// ! Converts script code into variable.
// > DoArgumentsClear
// > DoNameSet
static ReDefine::ScriptEditReturn DoVariable( ReDefine::ScriptEditAction& action, ReDefine::ScriptCode& code )
{
    if( code.IsFunction( nullptr ) )
    {
        if( action.CallEditDo( code, "DoArgumentsClear" ) != ReDefine::ScriptEditReturn::Success )
            return action.Invalid();
    }

    code.SetType( ReDefine::ScriptCode::Flag::VARIABLE );

    if( action.IsValues( nullptr, 1 ) )
    {
        if( action.CallEditDo( code, "DoNameSet", { action.Values[0] } ) != ReDefine::ScriptEditReturn::Success )
            return action.Invalid();
    }

    code.SetFlag( ReDefine::ScriptCode::Flag::REFRESH );

    return action.Success();
}

//

void ReDefine::InitScript()
{
    // must start with "If"
    EditIf["IfArgumentCondition"] = &IfArgumentCondition;
    EditIf["IfArgumentIs"] = &IfArgumentIs;
//  EditIf["IfArgumentNotValue"] = &IfArgumentNotValue;
    EditIf["IfArgumentValue"] = &IfArgumentValue;
    EditIf["IfArgumentsEqual"] = &IfArgumentsEqual;
    EditIf["IfArgumentsSize"] = &IfArgumentsSize;
    EditIf["IfEdited"] = &IfEdited;
//  EditIf["IfFileDefined"] = &IfFileDefined;
    EditIf["IfFileName"] = &IfFileName;
    EditIf["IfFunction"] = &IfFunction;
    EditIf["IfName"] = &IfName;
//  EditIf["IfNotEdited"] = &IfNotEdited;
    EditIf["IfOperator"] = &IfOperator;
    EditIf["IfOperatorName"] = &IfOperatorName;
    EditIf["IfOperatorValue"] = &IfOperatorValue;
    EditIf["IfReturnType"] = &IfReturnType;
    EditIf["IfVariable"] = &IfVariable;

    // must start with "Do"
    EditDo["DoArgumentCount"] = &DoArgumentCount;
    EditDo["DoArgumentLookup"] = &DoArgumentLookup;
    EditDo["DoArgumentResult"] = &DoArgumentResult;
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
    EditDo["DoFunctionAround"] = &DoFunctionAround;
    EditDo["DoFunctionAroundArgument"] = &DoFunctionAroundArgument;
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
    EditOnDemand.clear();

    DebugChanges = ScriptDebugChanges::NONE;
    UseParser = false;
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

                bool         ignore = false, before = false, after = false, demand = false;
                unsigned int priority = 100;

                for( const auto& action : Config->GetStrVec( section, name ) )
                {
                    if( action.empty() )
                        continue;

                    // split name from arguments
                    std::vector<std::string> vals;
                    std::vector<std::string> arg = TextGetSplitted( action, ':' );
                    if( arg.empty() )
                    {
                        DEBUG( __FUNCTION__, "IGNORE [%s]", action.c_str() );
                        ignore = true;
                        break;
                    }
                    else if( arg.size() == 1 )
                    {
                        // do nothing
                    }
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
                    else if( arg[0] == "RunOnDemand" )
                        demand = true;
                    else if( arg[0].length() >= 3 && arg[0].substr( 0, 2 ) == "If" )
                    {
                        ScriptEdit::Action condition;
                        condition.Name = arg[0];
                        condition.Values = vals;
                        condition.Negate = false;

                        edit.Conditions.push_back( condition );
                    }
                    else if( arg[0].length() >= 4 && arg[0].substr( 0, 3 ) == "!If" )
                    {
                        ScriptEdit::Action condition;
                        condition.Name = arg[0].substr( 1 );
                        condition.Values = vals;
                        condition.Negate = true;

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

                    if( (arg[0] == "RunBefore" || arg[0] == "RunAfter" || arg[0] == "RunOnDemand") && !vals.empty() && !vals[0].empty() )
                    {
                        int tmpriority;
                        if( arg[0] == "RunOnDemand" )
                        {
                            WARNING( __FUNCTION__, "script edit<%s> ignored : edits running on demand cannot set priority", name.c_str() );
                            ignore = true;
                            break;
                        }
                        else if( !TextGetInt( vals[0], tmpriority ) || tmpriority < 0 )
                        {
                            WARNING( __FUNCTION__, "script edit<%s> ignored : invalid priority<%s>", name.c_str(), vals[0].c_str() );
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

                if( !before && !after && !demand )
                {
                    WARNING( nullptr, "script edit<%s> ignored : at least one of 'RunBefore' / 'RunAfter' / 'RunOnDemand' must be set", edit.Name.c_str() );
                    ignore = true;
                }

                if( (before || after) && edit.Conditions.empty() )
                {
                    WARNING( nullptr, "script edit<%s> ignored : 'RunBefore' / 'RunAfter' edits require at least one condition", edit.Name.c_str() );
                    ignore = true;
                }

                if( (before || after) && edit.Results.empty() )
                {
                    WARNING( nullptr, "script edit<%s> ignored : 'RunBefore' / 'RunAfter' edits require at least one result", edit.Name.c_str() );
                    ignore = true;
                }

                if( demand && edit.Conditions.empty() && edit.Results.empty() )
                {
                    WARNING( nullptr, "script edit<%s> ignored : 'RunOnDemand' edits require at least one result or condition", edit.Name.c_str() );
                    ignore = true;
                }

                if( (before || after) && demand )
                {
                    WARNING( nullptr, "script edit<%s> ignored : 'RunOnDemand' edits cannot be used together with 'RunBefore' or 'RunAfter'", edit.Name.c_str() );
                    ignore = true;
                }

                if( ignore )
                    continue;

                if( before )
                    EditBefore[priority].push_back( edit );
                if( after )
                    EditAfter[priority].push_back( edit );
                if( demand )
                    EditOnDemand[priority].push_back( edit );
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
    else if( !std::filesystem::exists( path ) )
    {
        WARNING( __FUNCTION__, "script<%s> path<%s> does not exists", filename.c_str(), path.c_str() );
        return;
    }
    else if( !std::filesystem::is_directory( path ) )
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
                std::string fname = filename;
                Parent->DEBUG( __FUNCTION__, "%s", fname.c_str() );

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
        parser.Log.Cache = false;
        parser.Loader = loader;

        Parser::File file;

        // show time!
        auto              read = std::chrono::system_clock::now();
        std::vector<char> data;
        DEBUG( nullptr, "READ! %s %s", path.c_str(), filename.c_str() );
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

    bool                 updateFile = false, conflict = false, restart = false, codeChanged = false;
    std::string          content, newline = ScriptFormattingUnix ? "\n" : "\r\n";
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
                        // code.ArgumentsTypes = it->second.ArgumentsTypes;
                        code.SetFunctionArgumentsTypes( it->second );
                    }
                }

                code.Change( "script code", code.GetFullString() );

                // "preprocess"
                ProcessScriptEdit( ScriptEditAction::Flag::BEFORE, EditBefore, code, restart );

                // "process"
                ProcessScriptReplacements( code );

                // "postprocess"
                ProcessScriptEdit( ScriptEditAction::Flag::AFTER, EditAfter, code, restart );

                // check for changes
                code.SetFullString();
                codeChanged = TextGetPacked( codeOld.Full ) != TextGetPacked( code.Full );

                // dump changelog
                if( code.Changes.size() >= 2 && (DebugChanges == ScriptDebugChanges::ALL || (DebugChanges == ScriptDebugChanges::ONLY_IF_CHANGED && codeChanged) ) )
                    code.ChangeLog();

                // update if needed
                if( ScriptFormattingForced || codeChanged )
                    line = TextGetReplaced( line, codeOld.Full, code.Full );

                // handle restart
                if( restart )
                    restartCount++;
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

    if( DebugChanges > ScriptDebugChanges::NONE )
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
                // code.ArgumentsTypes = it->second.ArgumentsTypes;
                code.SetFunctionArgumentsTypes( it->second );
            }
        }
    }

    if( code.IsFunction( nullptr ) )
    {
        ProcessFunctionArguments( code );

        if( DebugChanges > ScriptDebugChanges::NONE )
        {
            after = code.GetFullString();

            if( TextGetPacked( before ) != TextGetPacked( after ) )
                code.Change( "script replacement<function arguments>" + std::string( refresh ? " refresh" : "" ), after );
        }
    }

    if( code.IsVariable( nullptr ) || code.IsFunction( nullptr ) )
    {
        std::string replacement;

        if( DebugChanges > ScriptDebugChanges::NONE )
            before = code.GetFullString();

        if( !IsMysteryDefineType( code.ReturnType ) )
        {
            ProcessOperator( code.ReturnType, code );

            if( DebugChanges > ScriptDebugChanges::NONE )
                replacement = "operator";
        }
        // try to guess define name for right part
        else if( code.ReturnType == "?" && code.Operator.length() && code.OperatorArgument.length() )
        {
            ProcessValueGuessing( code.OperatorArgument );

            if( DebugChanges > ScriptDebugChanges::NONE )
                replacement = "guessing";
        }

        if( DebugChanges > ScriptDebugChanges::NONE )
        {
            after = code.GetFullString();

            if( TextGetPacked( before ) != TextGetPacked( after ) )
                code.Change( "script replacement<" + replacement + ">" + (refresh ? " refresh" : ""), after );
        }
    }
}

void ReDefine::ProcessScriptEdit( const ScriptEditAction::Flag& initFlag, const std::map<unsigned int, std::vector<ReDefine::ScriptEdit>>& edits, ReDefine::ScriptCode& codeOld, bool& restart, ScriptEdit::External& external /* = ScriptEdit::ExternalDummy */ )
{
    // editing must always works on backup to prevent massive screwup
    // original code will be updated only if there's no problems with *any* condition/result function
    // that, plus (intentional) massive spam in warning log should be enough to get user's attention (yeah, i don't belive that either... :P)
    ScriptCode        code = codeOld;
    const std::string timing = initFlag == ScriptEditAction::Flag::BEFORE ? "Before" : initFlag == ScriptEditAction::Flag::AFTER ? "After" : initFlag == ScriptEditAction::Flag::DEMAND ? "OnDemand" : "";

    for( const auto& it : edits )
    {
        for( const ScriptEdit& edit : it.second )
        {
            if( external.InUse() && edit.Name != external.Name )
                continue;

            const ScriptDebugChanges debug = edit.Debug ? ScriptDebugChanges::ALL : DebugChanges;
            ScriptEditReturn         editReturn = ScriptEditReturn::Invalid;
            ScriptEditAction::Flag   editFlag = initFlag;

            bool                     run = false, first = true;
            const std::string        change = "script edit<" +  timing + ":" + std::to_string( it.first ) + ":" + edit.Name + ">";
            const size_t             changesSize = code.Changes.size();
            std::string              log;

            // all conditions needs to be satisfied
            for( const ScriptEdit::Action& condition : edit.Conditions )
            {
                if( external.InUse() && !external.RunConditions )
                    break;

                ScriptEditAction editAction( this, condition, editFlag );
                editReturn = editAction.CallEditIf( code );

                if( editReturn != ScriptEditReturn::Invalid )
                {
                    if( editReturn == ScriptEditReturn::Success )
                        run = true;
                    else if( editReturn == ScriptEditReturn::Failure )
                        run = false;

                    if( condition.Negate )
                        run = !run;

                    // if( !external.Name.empty() )
                    //     DEBUG( __FUNCTION__, "Run external condition %s %s result = %s", edit.Name.c_str(), condition.Name.c_str(), run ? "Success" : "Failure" );
                    external.ReturnConditions = run ? ScriptEditReturn::Success : ScriptEditReturn::Failure;
                }
                else
                {
                    WARNING( nullptr, "script edit<%s> aborted : condition<%s> invalid", edit.Name.c_str(), condition.Name.c_str() );
                    run = false;

                    if( !external.Name.empty() )
                        DEBUG( __FUNCTION__, "Run external condition %s %s result = Invalid", edit.Name.c_str(), condition.Name.c_str() );
                    external.ReturnConditions = ScriptEditReturn::Invalid;
                }

                if( debug > ScriptDebugChanges::NONE && ( (first && run) || !first ) )
                    code.Change( change + " " + (condition.Negate ? "!" : "") + condition.Name + (!condition.Values.empty() ? (":" + TextGetJoined( condition.Values, "," ) ) : ""), run ? "true" : "false" );

                if( !run )
                    break;

                first = false;
            }

            if( external.InUse() && !external.RunConditions )
            {
                // Ignore default condition results when running results only
            }
            else if( !run )
            {
                if( debug == ScriptDebugChanges::ONLY_IF_CHANGED )
                    code.Changes.resize( changesSize );

                continue;
            }

            // you are Result, you must Do
            for( const ScriptEdit::Action& result : edit.Results )
            {
                if( external.InUse() && !external.RunResults )
                    continue;

                if( debug > ScriptDebugChanges::NONE )
                    log = " " + result.Name + (!result.Values.empty() ? (":" + TextGetJoined( result.Values, "," ) ) : "");

                ScriptEditAction editAction( this, result, editFlag );
                editReturn = editAction.CallEditDo( code );

                if( editReturn != ScriptEditReturn::Invalid )
                {
                    if( editReturn == ScriptEditReturn::Success )
                        run = true;
                    else if( editReturn == ScriptEditReturn::Failure )
                    {
                        WARNING( nullptr, "internal error : result reported failure" );
                        run = false;
                    }

                    external.ReturnResults = run ? ScriptEditReturn::Success : ScriptEditReturn::Invalid;
                }
                else
                {
                    run = false;

                    external.ReturnResults = ScriptEditReturn::Invalid;
                }

                if( !run )
                {
                    if( debug > ScriptDebugChanges::NONE )
                        code.Change( change + log, "(ERROR)" );

                    WARNING( nullptr, "script edit<%s> aborted : result<%s> failed", edit.Name.c_str(), result.Name.c_str() );

                    return;
                }

                if( debug > ScriptDebugChanges::NONE )
                    code.Change( change + log, code.GetFullString() );

                code.SetFlag( ScriptCode::Flag::EDITED );

                // handle restart
                if( editAction.IsFlag( ScriptEditAction::Flag::RESTART ) )
                {
                    // handle restart+refresh
                    if( code.IsFlag( ScriptCode::Flag::REFRESH ) )
                    {
                        code.UnsetFlag( ScriptCode::Flag::REFRESH );
                        ProcessScriptReplacements( code, true );
                    }

                    // push changes
                    codeOld = code;
                    restart = true;

                    return;
                }
            }     // for( const ScriptEdit::Action& result : edit.Results )

            // handle refresh
            if( code.IsFlag( ScriptCode::Flag::REFRESH ) )
            {
                code.UnsetFlag( ScriptCode::Flag::REFRESH );
                ProcessScriptReplacements( code, true );
            }
        }     // for( const ScriptEdit& edit : it.second )
    }         // for( const auto& it : edits )

    // push changes
    codeOld = code;
}
