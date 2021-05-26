#include <algorithm>
#include <filesystem>
#include <fstream>

#include "Ini.h"

#include "ReDefine.h"

//

ReDefine::SStatus::SCurrent::SCurrent() :
    File( "" ),
    Line( "" ),
    LineNumber( 0 )
{}

void ReDefine::SStatus::SCurrent::Clear()
{
    File = Line = "";
    LineNumber = 0;
}

//

ReDefine::SStatus::SProcess::SProcess() :
    Files( 0 ),
    Lines( 0 ),
    FilesChanges( 0 ),
    LinesChanges( 0 )
{}

void ReDefine::SStatus::SProcess::Clear()
{
    Files = FilesChanges = Lines = LinesChanges = 0;
    Counters.clear();
}

//

void ReDefine::SStatus::Clear()
{
    Current.Clear();
    Process.Clear();
}

//

ReDefine::ReDefine() :
    Config( nullptr ),
    Dev( false ),
    LogFile( "ReDefine.log" ),
    LogWarning( "ReDefine.WARNING.log" ),
    LogDebug( "ReDefine.DEBUG.log" ),
    DebugChanges( ScriptDebugChanges::NONE ),
    UseParser( false ),
    ScriptFormattingForced( false ),
    ScriptFormattingUnix( false ),
    ScriptFormatting( ScriptCode::Format::DEFAULT )
{}

ReDefine::~ReDefine()
{
    Finish();
}

void ReDefine::Init()
{
    Finish();

    // create config
    Config = new Ini();
    Config->KeepKeysOrder = true;

    // extern initialization
    InitOperators();
    InitScript();
}

void ReDefine::Finish()
{
    if( Config )
    {
        delete Config;
        Config = nullptr;
    }

    Status.Clear();

    // extern cleanup
    FinishDefines();
    FinishFunctions();
    FinishOperators();
    FinishRaw();
    FinishScript();
    FinishVariables();
}

void ReDefine::RemoveLogs()
{
    // remove logfiles from previous run

    if( !LogFile.empty() && std::filesystem::exists( LogFile ) )
        std::filesystem::remove( LogFile );

    if( !LogWarning.empty() && std::filesystem::exists( LogWarning ) )
        std::filesystem::remove( LogWarning );

    if( !LogDebug.empty() && std::filesystem::exists( LogDebug ) )
        std::filesystem::remove( LogDebug );
}

// files reading

bool ReDefine::ReadFile( const std::string& filename, std::vector<std::string>& lines )
{
    lines.clear();

    const std::string file = TextGetReplaced( filename, "\\", "/" );
    if( !std::filesystem::exists( file ) )
    {
        WARNING( nullptr, "cannot find file<%s>", file.c_str() );
        return false;
    }

    // don't waste time on empty files
    // also, while( !std::ifstream::eof() ) goes wild on empty files
    uintmax_t size = std::filesystem::file_size( file );
    if( !size )
        return true;

    std::ifstream fstream;
    fstream.open( file, std::ios_base::in | std::ios_base::binary );

    bool result = fstream.is_open();
    if( result )
    {
        if( size >= 3 )
        {
            // skip bom
            char bom[3] = { 0, 0, 0 };
            fstream.read( bom, sizeof(bom) );
            if( bom[0] != (char)0xEF || bom[1] != (char)0xBB || bom[2] != (char)0xBF )
                fstream.seekg( 0, std::ifstream::beg );
            else
            {
                size -= 3;
                if( !size )
                    return true;
            }
        }

        std::string line;
        while( std::getline( fstream, line, '\n' ) )
        {
            lines.push_back( TextGetReplaced( line, "\r", "" ) );
        }
    }
    else
        WARNING( nullptr, "cannot read file<%s>", file.c_str() );

    return result;
}

bool ReDefine::ReadFile( const std::string& filename, std::vector<char>& data )
{
    data.clear();

    const std::string file = TextGetReplaced( filename, "\\", "/" );
    if( !std::filesystem::exists( file ) )
    {
        WARNING( nullptr, "cannot find file<%s>", file.c_str() );
        return false;
    }

    // don't waste time on empty files
    std::uintmax_t size = std::filesystem::file_size( file );
    if( size == 0 )
        return true;

    std::ifstream fstream;
    fstream.open( file, std::ios_base::in | std::ios_base::binary );

    bool result = fstream.is_open();
    if( result )
    {
        // skip bom
        char bom[3] = { 0, 0, 0 };
        fstream.read( bom, sizeof(bom) );
        if( bom[0] != (char)0xEF || bom[1] != (char)0xBB || bom[2] != (char)0xBF )
            fstream.seekg( 0, std::ifstream::beg );
        else
            size -= 3;

        data.resize( size );
        fstream.read( &data[0], size );
    }
    else
        WARNING( nullptr, "cannot read file<%s>", file.c_str() );

    return result;
}

bool ReDefine::ReadConfig( const std::string& defines, const std::string& variablePrefix, const std::string& functionPrefix, const std::string& raw, const std::string& script )
{
    if( !defines.empty() && !ReadConfigDefines( defines ) )
        return false;

    if( !variablePrefix.empty() && !ReadConfigVariables( variablePrefix ) )
        return false;

    if( !functionPrefix.empty() && !ReadConfigFunctions( functionPrefix ) )
        return false;

    if( !raw.empty() && !ReadConfigRaw( raw ) )
        return false;

    if( !script.empty() && !ReadConfigScript( script ) )
        return false;

    return true;
}

// files processing

void ReDefine::ProcessHeaders( const std::string& path )
{
    // move program defines to function scope
    DefinesMap programDefines = ProgramDefines;
    ProgramDefines.clear();

    // don't give up if there's something wrong with path - defines can be added via config, and replacements might still work
    // additionally, it allows config-only setup, which might be a good idea in case main application wants to parse headers on its own and add them to config
    // (which isn't such a bad idea - current headers reader is quite bad, as it doesn't understand /* long comments */ yet)
    if( path.empty() )
        WARNING( __FUNCTION__, "headers path is empty" );
    else if( !std::filesystem::exists( path ) )
        WARNING( __FUNCTION__, "headers path<%s> does not exists", path.c_str() );
    else if( !std::filesystem::is_directory( path ) )
        WARNING( __FUNCTION__, "headers path<%s> is not a directory", path.c_str() );
    else
    {
        for( const auto& header : Headers )
        {
            ProcessHeader( path, header );
        }
    }

    // won't need that until next ReadConfig()/ReadConfigDefines() call
    Headers.clear();

    // restore program defines
    // if type has been added by header, put it in RegularDefines (possibly overriding value from header),
    // otherwise, use ProgramDefines
    for( const auto& itProg : programDefines )
    {
        std::string            type = itProg.first, group;
        std::string::size_type pos = type.find_first_of( ":" );
        if( pos != std::string::npos )
        {
            group = type.substr( pos + 1 );
            type.erase( pos );
        }

        auto itRegular = RegularDefines.find( type );
        bool isRegular = IsRegularDefineType( type );

        if( !isRegular && !group.empty() )
        {
            auto itVirtual = VirtualDefines.find( group );
            if( itVirtual != VirtualDefines.end() )
            {
                if( std::find( itVirtual->second.begin(), itVirtual->second.end(), type ) == itVirtual->second.end() )
                {
                    LOG( "Added %s +> %s", type.c_str(), group.c_str() ); // existing
                    itVirtual->second.push_back( type );
                }
            }
            else
            {
                LOG( "Added %s +> %s", type.c_str(), group.c_str() ); // new
                VirtualDefines[group].push_back( type );
            }
        }

        for( const auto& itVal : itProg.second )
        {
            if( isRegular )
                itRegular->second[itVal.first] = itVal.second;
            else
                ProgramDefines[type][itVal.first] = itVal.second;

            LOG( "Added %s define ... %s = %d", type.c_str(), itVal.second.c_str(), itVal.first );
        }
    }

    std::map<std::string, std::string>   validVariables;
    std::map<std::string, FunctionProto> validFunctions;

    // validate variables configuration
    // virtual ? types are not valid for variables

    for( const auto& var : VariablesPrototypes )
    {
        if( !IsDefineType( var.second ) )
        {
            WARNING( __FUNCTION__, "unknown define type<%s> : variable<%s>", var.second.c_str(), var.first.c_str() );
            continue;
        }

        LOG( "Added variable ... [%s] %s", var.second.c_str(), var.first.c_str() );
        validVariables[var.first] = var.second;
    }


    // validate functions configuration
    // virtual ? types are valid for functions

    for( const auto& func : FunctionsPrototypes )
    {
        bool     valid = true;
        uint32_t argument = 0;

        if( !IsMysteryDefineType( func.second.ReturnType ) && !IsDefineType( func.second.ReturnType ) )
        {
            WARNING( __FUNCTION__, "unknown define type<%s> : function<%s> return type", func.second.ReturnType.c_str(), func.first.c_str() );
            valid = false;
        }

        for( const auto& type : func.second.ArgumentsTypes )
        {
            argument++;
            if( !IsMysteryDefineType( type ) && !IsDefineType( type ) )
            {
                WARNING( __FUNCTION__, "unknown define type<%s> : function<%s> argument<%u>", type.c_str(), func.first.c_str(), argument );
                valid = false;
            }
        }

        if( !valid )
            continue;

        LOG( "Added function ... [%s] %s(%s)", func.second.ReturnType.c_str(), func.first.c_str(), std::string( func.second.ArgumentsTypes.empty() ? "" : " " + TextGetJoined( func.second.ArgumentsTypes, ", " ) + " " ).c_str() );
        validFunctions[func.first] = func.second;
    }

    for( const auto& type : VariablesGuessing )
    {
        if( !IsDefineType( type ) )
        {
            WARNING( __FUNCTION__, "unknown define type<%s> : guessing", type.c_str() );
            VariablesGuessing.clear(); // zero tolerance policy
            break;
        }
    }

    if( !VariablesGuessing.empty() )
        LOG( "Added guessing ... %s", TextGetJoined( VariablesGuessing, ", " ).c_str() );

    // keep valid settings only
    VariablesPrototypes = validVariables;
    FunctionsPrototypes = validFunctions;

    // log raw replacement
    for( const auto& from : Raw )
    {
        LOG( "Added raw ... %s", from.first.c_str() );
    }

    // log script editing

    for( const auto& it : EditBefore )
    {
        for( const ScriptEdit& before : it.second )
        {
            LOG( "Added preprocess action ... %s", before.Name.c_str() );
        }
    }

    for( const auto& it : EditAfter )
    {
        for( const ScriptEdit& after : it.second )
        {
            LOG( "Added postprocess action ... %s", after.Name.c_str() );
        }
    }

    for( const auto& it : EditOnDemand )
    {
        for( const ScriptEdit& demand : it.second )
        {
            LOG( "Added on-demand action ... %s", demand.Name.c_str() );
        }
    }
}

void ReDefine::ProcessScripts( const std::string& path, const bool readOnly /* = false */ )
{
    if( path.empty() )
    {
        WARNING( __FUNCTION__, "scripts path is empty" );
        return;
    }
    else if( !std::filesystem::exists( path ) )
    {
        WARNING( __FUNCTION__, "scripts path<%s> does not exists", path.c_str() );
        return;
    }
    else if( !std::filesystem::is_directory( path ) )
    {
        WARNING( __FUNCTION__, "scripts path<%s> is not a directory", path.c_str() );
        return;
    }

    LOG( "Process scripts%s ...", readOnly ? " (read only)" : "" );

    std::vector<std::string> scripts;

    for( const auto& file : std::filesystem::recursive_directory_iterator( path ) )
    {
        if( !std::filesystem::is_regular_file( file ) ) // TODO? symlinks
            continue;

        if( TextGetLower( file.path().extension().string() ) != ".ssl" )
            continue;

        scripts.push_back( file.path().string().substr( path.length(), file.path().string().length() - path.length() ) );
        scripts.back().erase( 0, scripts.back().find_first_not_of( "\\/" ) );  // trim left
    }

    std::sort( scripts.begin(), scripts.end() );

    for( auto& script : scripts )
    {
        ProcessScript( path, script, readOnly );
    }
}
