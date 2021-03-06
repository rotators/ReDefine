#include <cerrno>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <sstream>

#include "ReDefine.h"

namespace
{
    static const std::regex IsBlank( "^[\\t\\ ]*$" );
    static const std::regex IsComment( "^[\\t\\ ]*\\/\\/" );
    static const std::regex IsDefine( "^[\\t\\ ]*\\#define[\\t\\ ]+" );
    static const std::regex IsInt( "^[\\-]?[0-9]+$" );
    static const std::regex IsConflict( "^[\\<]+ (HEAD|\\.mine).*$" );

    static const std::regex GetVariables( "([A-Za-z0-9_]+)[\\t\\ ]*([\\:\\=\\!\\<\\>\\+]+|[Bb][Ww][a-z]+)[\\t\\ ]*([\\-]?[A-Za-z0-9\\_]+)" );
    static const std::regex GetVariablesSimple( "([A-Za-z0-9_]+)[\\t\\ ]*;" );

    static const std::regex GetFunctions( "([A-Za-z0-9_]+)\\(" );
    static const std::regex GetFunctionsQuotedText( "\".*?\"" );
}

bool ReDefine::TextIsBlank( const std::string& text )
{
    return std::regex_match( text, IsBlank );
}

bool ReDefine::TextIsComment( const std::string& text )
{
    return std::regex_search( text, IsComment );
}

bool ReDefine::TextIsInt( const std::string& text )
{
    return std::regex_match( text, IsInt );
}

bool ReDefine::TextIsConflict( const std::string& text )
{
    if( text.front() != '<' )
        return false;

    return std::regex_match( text, IsConflict );
}

std::string ReDefine::TextGetFilename( const std::string& path, const std::string& filename )
{
    std::string spath = path;
    std::string sfilename = filename;

    sfilename.erase( 0, sfilename.find_first_not_of( "\\/" ) ); // trim left
    spath.erase( spath.find_last_not_of( "\\/" ) + 1 );         // trim right

    std::filesystem::path full = std::filesystem::path( spath ) / sfilename;

    // DEBUG( __FUNCTION__, "[%s][%s] -> [%s][%s] -> [%s]", path.c_str(), filename.c_str(), spath.c_str(), sfilename.c_str(), full.string().c_str() );

    return full.string();
}

bool ReDefine::TextGetInt( const std::string& text, int& result, const uint8_t& base /* = 10 */ )
{
    // https://stackoverflow.com/a/6154614
    const char* cstr = text.c_str();
    char*       end;
    long        l;
    errno = 0;
    l = strtol( cstr, &end, base );
    if( ( (errno == ERANGE && l == LONG_MAX) || l > INT_MAX ) ||
        ( (errno == ERANGE && l == LONG_MIN) || l < INT_MIN ) ||
        (*cstr == '\0' || *end != '\0') )
        return false;

    result = l;

    return true;
}

std::string ReDefine::TextGetJoined( const std::vector<std::string>& text, const std::string& delimeter )
{
    static const std::string empty;

    switch( text.size() )
    {
        case 0:
            return empty;
        case 1:
            return text[0];
        default:
            std::ostringstream oss;
            copy( text.begin(), text.end() - 1, std::ostream_iterator<std::string>( oss, delimeter.c_str() ) );
            oss << *text.rbegin();
            return oss.str();
    }
}

std::string ReDefine::TextGetLower( const std::string& text )
{
    std::string result = text;

    transform( result.begin(), result.end(), result.begin(), ::tolower );

    return result;
}

std::string ReDefine::TextGetPacked( const std::string& text )
{
    std::string result;

    result = TextGetReplaced( text,  "\t", " " );
    result = TextGetReplaced( result, " ", "" );

    return result;
}

std::string ReDefine::TextGetReplaced( const std::string& text, const std::string& from, const std::string& to )
{
    std::string                 result;

    std::string::const_iterator textCurrent = text.begin();
    std::string::const_iterator textEnd = text.end();
    std::string::const_iterator next = std::search( textCurrent, textEnd, from.begin(), from.end() );

    while( next != textEnd )
    {
        result.append( textCurrent, next );
        result.append( to );
        textCurrent = next + from.size();
        next = std::search( textCurrent, textEnd, from.begin(), from.end() );
    }

    result.append( textCurrent, next );

    return result;
}

std::vector<std::string> ReDefine::TextGetSplitted( const std::string& text, const char& separator, uint8_t limit /* = 0 */ )
{
    std::vector<std::string> result;

    if( !text.empty() )
    {
        std::string        tmp;
        std::istringstream f( text );
        while( std::getline( f, tmp, separator ) )
        {
            if( separator != ' ' )
                tmp = TextGetTrimmed( tmp );

            if( limit > 0 && result.size() >= limit )
                result.back() += separator + tmp;
            else
                result.push_back( tmp );
        }
    }

    return result;
}

std::string ReDefine::TextGetTrimmed( const std::string& text )
{
    std::string result = text;

    // C++14
    // result.erase( result.begin(), find_if( result.begin(), result.end(), std::not1( std::ptr_fun<int, int>( isspace ) ) ) );
    // result.erase( find_if( result.rbegin(), result.rend(), std::not1( std::ptr_fun<int, int>( isspace ) ) ).base(), result.end() );

    // C++17+
    result.erase( result.begin(), std::find_if( result.begin(), result.end(), [] ( char c ) {
        return !isspace( c );
    } ) );
    result.erase( std::find_if( result.rbegin(), result.rend(), [] ( char c ) {
        return !std::isspace( c );
    } ).base(), result.end() );

    return result;
}

//

bool ReDefine::TextIsDefine( const std::string& text )
{
    return std::regex_search( text, IsDefine );
}

bool ReDefine::TextGetDefineInt( const std::string& text, const std::regex& re, std::string& name, int& value )
{
    std::smatch match;
    if( std::regex_search( text, match, re ) )
    {
        name = match.str( 1 );
        TextGetInt( match.str( 2 ), value );
        return true;
    }

    return false;
}

bool ReDefine::TextGetDefineString( const std::string& text, const std::regex& re, std::string& name, std::string& value )
{
    std::smatch match;
    if( std::regex_search( text, match, re ) )
    {
        name = match.str( 1 );
        value = match.str( 2 );
        return true;
    }

    return false;
}

std::regex ReDefine::TextGetDefineIntRegex( std::string prefix, std::string suffix, bool paren )
{
    if( !prefix.empty() )
        prefix += "_";
    if( !suffix.empty() )
        suffix = "_" + suffix;

    return std::regex( "^[\\t\\ ]*\\#define[\\t\\ ]+(" + prefix + "[A-Za-z0-9_]+" + suffix + ")[\\t\\ ]+" + (paren ? "\\(" : "") + "([\\-]?[0-9]+)" + (paren ? "\\)" : "") );
}

std::regex ReDefine::TextGetDefineStringRegex( std::string prefix, std::string suffix, bool paren, const std::string& re )
{
    if( !prefix.empty() )
        prefix += "_";
    if( !suffix.empty() )
        suffix = "_" + suffix;

    return std::regex( "^[\\t\\ ]*\\#define[\\t\\ ]+(" + prefix + "[A-Za-z0-9_]+" + suffix + ")[\\t\\ ]+" + (paren ? "\\(" : "") + "(" + re + ")" + (paren ? "\\)" : "") );
}

//

uint32_t ReDefine::TextGetVariables( const std::string& text, std::vector<ReDefine::ScriptCode>& result )
{
    uint32_t count = 0;

    // variable <operator> <operator value>
    for( auto it = std::sregex_iterator( text.begin(), text.end(), GetVariables ), end = std::sregex_iterator(); it != end; ++it )
    {
        ScriptCode variable( ScriptCode::Flag::VARIABLE );

        variable.Full = it->str();
        variable.Name = it->str( 1 );
        variable.Operator = it->str( 2 );
        variable.OperatorArgument = it->str( 3 );

        if( !IsOperator( variable.Operator ) )
        {
            /*
               if( TextGetLower( variable.Operator ).starts_with( "bw" ) )
               {
                DEBUG( __FUNCTION__, "Unknown bitwise operator<%s> : %s %s %s", variable.Operator.c_str(), variable.Name.c_str(), variable.Operator.c_str(), variable.OperatorArgument.c_str() );
               }
               else
               {
                DEBUG( __FUNCTION__, "Unknown operator<%s> : %s %s %s", variable.Operator.c_str(), variable.Name.c_str(), variable.Operator.c_str(), variable.OperatorArgument.c_str() );
               }
             */
            continue;
        }

        result.push_back( variable );
        count++;

        // DEBUG(__FUNCTION__, "VAR C!<%s>", variable.Name.c_str());
    }

    // variable;
    for( auto it = std::sregex_iterator( text.begin(), text.end(), GetVariablesSimple ), end = std::sregex_iterator(); it != end; ++it )
    {
        ScriptCode variable( ScriptCode::Flag::VARIABLE );

        variable.Full = it->str( 1 );
        variable.Name = it->str( 1 );

        if( TextIsInt( variable.Name ) )
            continue;

        result.push_back( variable );
        count++;

        // DEBUG( __FUNCTION__, "VAR S!<%s>", variable.Name.c_str() );
    }

    return count;
}

uint32_t ReDefine::TextGetFunctions( const std::string& text, std::vector<ReDefine::ScriptCode>& result )
{
    uint32_t count = 0;

    uint32_t funcIdx = 0;
    for( auto it = std::sregex_iterator( text.begin(), text.end(), GetFunctions ), end = std::sregex_iterator(); it != end; ++it, funcIdx++ )
    {
        const std::string                 func = it->str( 1 );
        std::string                       full, arg, op, opArg;
        ScriptCode::Argument              argument;
        std::vector<ScriptCode::Argument> arguments;
        size_t                            stage = 0, funcStart = it->position(), funcLen = func.length() + 1, funcArgsLen = 0;
        int                               balance = 1;
        bool                              quote = false, quoteFound = false;

        for( size_t t = funcStart + funcLen, tLen = text.length(); t < tLen; t++, funcLen++ )
        {
            const char ch = text[t];

            if( ch == ';' && !quote )
            {
                full = text.substr( funcStart, funcLen );
                break;
            }

            // extract arguments
            if( stage == 0 )
            {
                // detect strings
                if( ch == '"' )
                {
                    quote = !quote;
                    quoteFound = true;
                }

                // ignore everything inside strings
                if( quote )
                {
                    arg += ch;
                    continue;
                }

                // balancing
                if( ch == '(' )
                    balance++;
                else if( ch == ')' )
                {
                    if( --balance <= 0 )
                    {
                        funcArgsLen = funcLen + 1;
                        full = text.substr( funcStart, funcArgsLen );
                        balance = 0;
                        stage++;
                        continue;
                    }
                }
                // update arguments list
                else if( ch == ',' && balance == 1 )
                {
                    argument.Raw = arg;
                    argument.Arg = TextGetTrimmed( arg );
                    argument.Type = "?";
                    arguments.push_back( argument );

                    arg.clear();
                    continue;
                }

                // update current argument
                arg += ch;
            }
            // extract operator (if any)
            else if( stage == 1 )
            {
                if( ch == ' ' || ch == '\t' )
                {
                    if( op.length() )
                    {
                        if( !IsOperator( op ) )
                        {
                            // TODO? support bw*
                            // if( op.length() > 2 && op.substr( 0, 2 ) == "bw" )
                            //     DEBUG( __FUNCTION__, "IGNORED bitwise<%s> :: %s", op.c_str(), text.c_str() );

                            op.clear();
                            break;
                        }
                        stage++;
                    }

                    continue;
                }
                else if( (ch == ')' || ch == ',') && balance == 0 )
                    break;

                op += ch;
            }
            // extract operator argument
            else
            {
                // detect strings
                if( ch == '"' )
                {
                    quote = !quote;
                    quoteFound = true;
                }

                // ignore everything inside strings
                if( quote )
                {
                    opArg += ch;
                    continue;
                }

                // balancing
                if( ch == '(' )
                    balance++;
                else if( ch == ')' )
                {
                    // this will extract pieces like 'f1(arg) + f2(arg) + f3(arg)' as 'f1(arg) + f2(arg)', ignoring 'f3()' part and anything after
                    // which is fine, i guess, as we can't do anything with code like 'x != 2 + random(2,4) + 2' anyway
                    // script edits checking OperatorArgument should be fine too, as they're not allowed to use spaces :>
                    if( --balance <= 0 )
                    {
                        full = text.substr( funcStart, funcLen + (static_cast<int64_t>(balance) + 1) );
                        if( balance == 0 )
                            opArg += ch;
                        break;
                    }
                }

                // detect operator argument end
                if( ch == ' ' )
                {
                    if( text.substr( t + 1, 4 ) == "then" || text.substr( t + 1, 2 ) == "or" ) // unsafe
                    {
                        full = text.substr( funcStart, funcLen );
                        break;
                    }
                }
                // make sure inner function does not include argument of outer function
                // prevents extracting function as 'inner(some, arg) -3, 1' from 'outer(3, 2, inner(some, arg) - 3, 1)'
                else if( balance == 0 && ch == ',' )
                    break;

                full = text.substr( funcStart, funcLen + 1 );
                opArg += ch;
            }
        }

        // add last argument (if any)
        if( !arg.empty() )
        {
            argument.Arg = TextGetTrimmed( arg );
            argument.Raw = arg;
            argument.Type = "?";

            arguments.push_back( argument );
        }

        // validate quotes detection
        if( !quoteFound && std::count( full.begin(), full.end(), '"' ) )
        {
            DEBUG( __FUNCTION__, "QUOTE CHARACTER MISSED" );
            quoteFound = true;
        }

        // validate balancing
        bool parens = false;
        if( quoteFound )
        {
            std::string tmp = std::regex_replace( full, GetFunctionsQuotedText, "STRING" );
            // DEBUG( __FUNCTION__, "STRIP(%u) STRINGS [%s] -> [%s]", funcIdx, full.c_str(), tmp.c_str() );

            if( std::count( tmp.begin(), tmp.end(), '(' ) != std::count( tmp.begin(), tmp.end(), ')' ) )
                parens = true;
        }

        // validate operator
        if( !op.empty() && opArg.empty() )
        {
            // DEBUG( __FUNCTION__, "STRIP(%u) OPERATOR(%s) [%s] -> [%s] :: %s", funcIdx, op.c_str(), full.c_str(), full.substr( 0, funcArgsLen ).c_str(), text.c_str() );
            full = full.substr( 0, funcArgsLen );
            op.clear();
            opArg.clear();
        }

        if( op.length() && opArg.length() && opArg.back() == '\\' )
        {
            full = full.substr( 0, funcArgsLen );
            op.clear();
            opArg.clear();
        }

        // check if function looks like, well, a function
        static const bool funcDebug = false;
        bool              ignore =  balance > 0 || quote || parens;
        if( funcDebug || ignore )
        {
            bool spam = func != "for" && func != "if";

            if( Dev && spam )
            {
                SStatus::SCurrent prev = Status.Current;

                Status.Current.Line = text;

                DEBUG( __FUNCTION__, "FUNCTION(%u)", funcIdx );
                if( ignore )
                    DEBUG( __FUNCTION__, "IGNORED(%s%s%s)", balance ? std::to_string( balance ).c_str() : "", quote ? "Q" : "", parens ? "P" : "" );

                Status.Current.Clear();

                std::vector<std::string> argumentsVec( arguments.size() );
                for( const ScriptCode::Argument& _argument : arguments )
                {
                    argumentsVec.push_back( _argument.Arg );
                }

                DEBUG( __FUNCTION__, "\tcalc[%s]", text.substr( funcStart, funcLen ).c_str() );
                DEBUG( __FUNCTION__, "\tfull[%s] b=%d", full.c_str(), balance );
                DEBUG( __FUNCTION__, "\tfunc[%s] args[%s] op[%s] opArg[%s] ", func.c_str(), TextGetJoined( argumentsVec, "|" ).c_str(), op.c_str(), opArg.c_str() );

                Status.Current = prev;
            }

            if( ignore )
                continue;
        }

        // update result
        ScriptCode function( ScriptCode::Flag::FUNCTION );

        function.Full = full;
        function.Name = func;
        function.ReturnType = "?";
        function.Arguments = arguments;
        function.Operator = TextGetTrimmed( op );
        function.OperatorArgument = TextGetTrimmed( opArg );

        result.push_back( function );
        count++;
    }

    return count;
}
