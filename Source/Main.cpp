#include "FOClassic/CommandLine.h"
#include "FOClassic/Ini.h"

#include "ReDefine.h"

void Usage( ReDefine* redefine )
{
    redefine->SHOW( " " );
    redefine->SHOW( "Usage: ReDefine [options]" );
    redefine->SHOW( " " );
    redefine->SHOW( "OPTIONS" );
    redefine->SHOW( " " );
    redefine->SHOW( "  --help                     Short summary of available options" );
    redefine->SHOW( "  --config [filename]        Changes location of configuration file (default: ReDefine.cfg)" );
    redefine->SHOW( "  --headers [directory]      Changes location of scripts headers directory (default: current directory)" );
    redefine->SHOW( "  --scripts[directory]       Changes location of scripts directory (default: current directory)" );
    redefine->SHOW( "  --log-file [filename]      Changes location of general logfile (default: %s)", redefine->LogFile.c_str() );
    redefine->SHOW( "  --log-warning [filename]   Changes location of warnings logfile (default: %s)", redefine->LogWarning.c_str() );
    redefine->SHOW( "  --log-debug [filename]     Changes location of debug logfile (default: %s)", redefine->LogDebug.c_str() );
    redefine->SHOW( "  --ro, --read, --read-only  Enables read-only mode; scripts files won't be changed (default: disabled)" );
    redefine->SHOW( "  --debug-changes            Enables debug mode; (default: %s)", redefine->DebugChanges ? "enabled" : "disabled" );
    #if defined (HAVE_PARSER)
    redefine->SHOW( "  --parser" );
    #endif
    redefine->SHOW( " " );
}

int main( int argc, char** argv )
{
    int result = EXIT_SUCCESS;

    // boring stuff
    std::setvbuf( stdout, NULL, _IONBF, 0 );
    CmdLine*  cmd = new CmdLine( argc, argv );
    ReDefine* redefine = new ReDefine();

    if( cmd->IsOption( "help" ) )
    {
        Usage( redefine );

        delete cmd;
        delete redefine;

        return result;
    }

    // exciting stuff
    const bool readOnly = cmd->IsOption( "ro" ) || cmd->IsOption( "read" ) || cmd->IsOption( "read-only" );

    redefine->SHOW( "ReDefine <3 FO1@2" );
    redefine->SHOW( " " );

    //
    // initialization
    //
    // Init()    recreates all internals
    // Finish()  does cleanup only; called by Init() and destructor
    //
    // both functions can be called at any point to completely reset object
    //

    redefine->Init();

    std::string       config = "ReDefine.cfg";
    const std::string section = "ReDefine";

    // override configuration filename from command line
    if( !cmd->IsOptionEmpty( "config" ) )
        config = cmd->GetStr( "config" );

    //
    // load configuration
    // config can be either loaded from ini-like file,
    // or set "manually" if main application is storing configuration in different format
    //
    // in both cases it has to be done before any ReadConfig*() call(s), which checks ReDefine::Config content without touching any files
    //

    if( redefine->Config->LoadFile( config ) )
    {
        //
        // read logfiles configuration
        // everything
        // empty Log* setting disables saving to file
        //

        // regular log
        // used to display active configuration (skipping invalid entries) and changes in scripts
        //
        // changes which are not logged, but are applied only if script code is changed:
        // - newline changes
        // - side-cleanup: automagic clearing of lines with spaces/tabs only, removing extra tabs/spaces at end of line, removing extra newlines at end of file, etc.
        if( redefine->Config->IsSectionKey( section, "LogFile" ) )
        {
            if( redefine->Config->IsSectionKeyEmpty( section, "LogFile" ) )
                redefine->LogFile.clear();
            else
                redefine->LogFile = redefine->Config->GetStr( section, "LogFile", redefine->LogFile );
        }

        if( !cmd->IsOptionEmpty( "log-file" ) )
            redefine->LogFile = cmd->GetStr( "log-file", redefine->LogFile );

        // warnings log
        // used to display info whenever something goes wrong
        //
        // as ReDefine does not know the concept of errors, and tries really hard to keep going, it might result with flood of warnings if something important is broken
        if( redefine->Config->IsSectionKey( section, "LogWarning" ) )
        {
            if( redefine->Config->IsSectionKeyEmpty( section, "LogFile" ) )
                redefine->LogWarning.clear();
            else
                redefine->LogWarning = redefine->Config->GetStr( section, "LogWarning", redefine->LogWarning );
        }

        if( !cmd->IsOptionEmpty( "log-warning" ) )
            redefine->LogWarning = cmd->GetStr( "log-warning", redefine->LogWarning );

        // debug log
        // used to display info whenever something goes wrong (and it's most likely developer fault) and during bug hunting
        //
        // additionally, debug log is used on demand for tracking step-by-step changes in scripts code
        if( redefine->Config->IsSectionKey( section, "LogDebug" ) )
        {
            if( redefine->Config->IsSectionKeyEmpty( section, "LogDebug" ) )
                redefine->LogDebug.clear();
            else
                redefine->LogDebug = redefine->Config->GetStr( section, "LogDebug", redefine->LogDebug );
        }

        if( !cmd->IsOptionEmpty( "log-debug" ) )
            redefine->LogDebug = cmd->GetStr( "log-debug", redefine->LogDebug );

        // remove old logfiles
        redefine->RemoveLogs();

        //
        // read directories configuration
        //

        std::string headers = redefine->Config->GetStr( section, "HeadersDir" );
        if( !cmd->IsOptionEmpty( "headers" ) )
            headers = cmd->GetStr( "headers" );

        std::string scripts = redefine->Config->GetStr( section, "ScriptsDir" );
        if( !cmd->IsOptionEmpty( "scripts" ) )
            scripts = cmd->GetStr( "scripts" );

        //
        // read additional configuration
        //

        bool debugChanges = redefine->Config->GetBool( section, "DebugChanges", false );
        if( cmd->IsOption( "debug-changes" ) )
            debugChanges = true;

        #if defined (HAVE_PARSER)
        bool parser = redefine->Config->GetBool( section, "Parser", false );
        if( cmd->IsOption( "parser" ) )
            parser = true;
        #endif

        int formatting = redefine->Config->GetInt( section, "FormatFunctions", -1 );
        if( formatting >= ReDefine::SCRIPT_FORMAT_MIN && formatting <= ReDefine::SCRIPT_FORMAT_MAX )
            redefine->ScriptFormatting = (ReDefine::ScriptCodeFormat)formatting;
        redefine->ScriptFormattingForced = redefine->Config->GetBool( section, "FormatFunctionsForced", redefine->ScriptFormattingForced );

        //
        // pre-validate config
        //
        if( headers.empty() )
        {
            redefine->WARNING( nullptr, "headers directory not set" );
            result = EXIT_FAILURE;
        }
        else if( scripts.empty() )
        {
            redefine->WARNING( nullptr, "scripts directory not set" );
            result = EXIT_FAILURE;
        }
        //
        // validate config and convert settings to internal structures
        //
        // sections can be loaded separetely by either calling ReadConfig() and changing unwanted sections names to empty strings,
        // or calling related ReadConfig*() function only
        //
        // defines section is required and needs to be processed before other settings
        //
        else if( redefine->ReadConfig( "Defines", "Variable", "Function", "Raw", "Script" ) )
        {
            // unload config
            // added here to make sure Process*() functions are independent of ReadConfig*()
            // in long-running/ui applications it might be a good idea to keep config loaded
            redefine->Config->Unload();

            //
            // additional tuning
            //

            redefine->DebugChanges = debugChanges;

            #if defined (HAVE_PARSER)
            redefine->UseParser = parser;
            #endif

            //
            // process headers
            //

            redefine->ProcessHeaders( headers );

            //
            // process scripts
            //

            redefine->ProcessScripts( scripts, readOnly );
        }
        else
        {
            redefine->Status.Current.File = config;
            redefine->WARNING( nullptr, "cannot parse config" );
            result = EXIT_FAILURE;
        }
    }
    else
    {
        redefine->Status.Current.File = config;
        redefine->WARNING( nullptr, "cannot read config" );
        result = EXIT_FAILURE;
    }

    //
    // show summary, if available
    //

    if( redefine->Status.Process.Lines && redefine->Status.Process.Files )
    {
        redefine->LOG( "Process scripts ... %u line%s in %u file%s",
                       redefine->Status.Process.Lines, redefine->Status.Process.Lines != 1 ? "s" : "",
                       redefine->Status.Process.Files, redefine->Status.Process.Files != 1 ? "s" : "" );
    }

    //
    // show changes summary, if available
    //

    if( redefine->Status.Process.LinesChanges && redefine->Status.Process.FilesChanges )
    {
        redefine->LOG( "Ch%sed scripts ... %u line%s in %u file%s%s",
                       readOnly ? "eck" : "ang",
                       redefine->Status.Process.LinesChanges, redefine->Status.Process.LinesChanges != 1 ? "s" : "",
                       redefine->Status.Process.FilesChanges, redefine->Status.Process.FilesChanges != 1 ? "s" : "",
                       readOnly ? " can be changed" : "" );
    }

    //
    // show counters, if available
    //

    if( redefine->Status.Process.Counters.size() )
    {
        for( const auto& counter : redefine->Status.Process.Counters )
        {
            // counters set by ReDefine when processing scripts
            bool internal = counter.first.front() == '!' && counter.first.back() == '!';
            if( internal )
            {
                redefine->WARNING( nullptr, " " );
                redefine->WARNING( nullptr, "%s (%u)", counter.first.substr( 1, counter.first.length() - 2 ).c_str(), counter.second.size() );
            }
            // counters set by script edit actions (DoNameCount, DoArgumentCount, etc.)
            else
            {
                redefine->LOG( " " );
                redefine->LOG( "Counter %s (%u)", counter.first.c_str(), counter.second.size() );
            }

            for( const auto& value : counter.second )
            {
                if( internal )
                    redefine->WARNING( nullptr, "        %s (%u hit%s)", value.first.c_str(), value.second, value.second != 1 ? "s" : "" );
                else
                    redefine->LOG( "        %s (%u hit%s)", value.first.c_str(), value.second, value.second != 1 ? "s" : "" );
            }
        }
    }

    // cleanup
    delete cmd;
    delete redefine;

    // go back to making Fallout mods
    return result;
}
