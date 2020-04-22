#include "Ini.h"

#include "ReDefine.h"

void ReDefine::FinishFunctions()
{
    FunctionsPrototypes.clear();
}

// reading

bool ReDefine::ReadConfigFunctions( const std::string& section )
{
    FinishFunctions();

    std::vector<std::string> keys;
    if( !Config->GetSectionKeys( section, keys ) )
        return true;

    for( const auto& name : keys )
    {
        std::vector<std::string> types = Config->GetStrVec( section, name );
        if( types.empty() )
            continue;

        FunctionProto function;
        function.ReturnType = "?";
        function.ArgumentsTypes = types;

        // if first argument is "[TYPE]", set function return type to "TYPE" and remove it from arguments list
        if( function.ArgumentsTypes.front().length() >= 3 && function.ArgumentsTypes.front().front() == '[' && function.ArgumentsTypes.front().back() == ']' )
        {
            function.ReturnType = function.ArgumentsTypes[0].substr( 1, function.ArgumentsTypes[0].length() - 2 );
            function.ArgumentsTypes.erase( function.ArgumentsTypes.begin() );
        }

        // type validation is part of ProcessHeaders(),
        // as at this point *Defines maps might not be initialized yet
        FunctionsPrototypes[name] = function;
    }

    return true;
}

// processing

void ReDefine::ProcessFunctionArguments( ReDefine::ScriptCode& function )
{
    // make sure function is preconfigured properly
    auto it = FunctionsPrototypes.find( function.Name );
    if( it != FunctionsPrototypes.end() )
    {
        const size_t expected = it->second.ArgumentsTypes.size(), found = function.Arguments.size();

        if( expected != found )
        {
            WARNING( __FUNCTION__, "invalid number of function<%s> arguments : expected<%u> found<%u>", function.Name.c_str(), expected, found );
            return;
        }
    }

    // called so late to always detect arguments count mismatch
    if( function.Arguments.empty() )
        return;

    for( size_t idx = 0, len = function.Arguments.size(); idx < len; idx++ )
    {
        if( function.Arguments[idx].Type.empty() ) // can happen by using DoArgumentsResize without DoArgumentChangeType or other edit combinations
        {
            WARNING( __FUNCTION__, "argument<%u> type not set", idx );
            continue;
        }
        else if( IsMysteryDefineType( function.Arguments[idx].Type ) )
        {
            // only "?" type should be guessed
            if( function.Arguments[idx].Type == "?" )
                ProcessValueGuessing( function.Arguments[idx].Arg );

            continue;
        }

        const std::string prevArgument = function.Arguments[idx].Arg;

        if( ProcessValue( function.Arguments[idx].Type, function.Arguments[idx].Arg ) )
        {
            // const std::string prevArgumentRaw = function.ArgumentsRaw[idx];
            function.Arguments[idx].Raw = TextGetReplaced( function.Arguments[idx].Raw, prevArgument, function.Arguments[idx].Arg );
            // DEBUG( __FUNCTION__, "[%s] -> [%s]", prevArgumentRaw.c_str(), function.ArgumentsRaw[idx].c_str() );
        }
    }
}
