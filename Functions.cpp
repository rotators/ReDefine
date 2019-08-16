#include "FOClassic/Ini.h"

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
    std::string  badArgs;
    unsigned int found = 0, expected = 0;

    // known functions
    auto it = FunctionsPrototypes.find( function.Name );
    if( it != FunctionsPrototypes.end() )
    {
        expected = it->second.ArgumentsTypes.size();

        if( expected != function.Arguments.size() )
        {
            badArgs = "arguments";
            found = function.Arguments.size();
        }
        else if( expected != function.ArgumentsTypes.size() )
        {
            badArgs = "arguments types";
            found = function.ArgumentsTypes.size();
        }
    }
    // unknown functions
    else
    {
        expected = function.Arguments.size();

        if( expected != function.ArgumentsTypes.size() )
        {
            badArgs = "arguments types";
            found = function.ArgumentsTypes.size();
        }
    }

    if( !badArgs.empty() )
    {
        WARNING( __FUNCTION__, "invalid number of function<%s> %s : found<%u> expected<%u>", function.Name.c_str(), badArgs.c_str(), found, expected );
        return;
    }

    // called so late to always detect count mismatch
    if( !function.Arguments.size() )
        return;

    for( unsigned int idx = 0, len = function.Arguments.size(); idx < len; idx++ )
    {
        if( function.ArgumentsTypes[idx].empty() ) // can happen by using DoArgumentsResize without DoArgumentChangeType or other edit combinations
        {
            WARNING( __FUNCTION__, "argument<%u> type not set", idx );
            continue;
        }
        else if( IsMysteryDefineType( function.ArgumentsTypes[idx] ) )
        {
            // only "?" type should be guessed
            if( function.ArgumentsTypes[idx] == "?" )
                ProcessValueGuessing( function.Arguments[idx] );

            continue;
        }

        ProcessValue( function.ArgumentsTypes[idx], function.Arguments[idx] );
    }
}
