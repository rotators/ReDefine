#include "Ini.h"

#include "ReDefine.h"

void ReDefine::FinishRaw()
{
    Raw.clear();
}

bool ReDefine::ReadConfigRaw( const std::string& section )
{
    FinishRaw();

    std::vector<std::string> keys;
    if( !Config->GetSectionKeys( section, keys ) )
        return true;

    for( const auto& from : keys )
    {
        const std::string to = Config->GetStr( section, from );

        if( to.empty() )
            continue;

        Raw[from] = to;
    }

    return true;
}

//

void ReDefine::ProcessRaw( std::string& line )
{
    for( const auto& replace : Raw )
    {
        std::string tmp = TextGetReplaced( line, replace.first, replace.second );
        line = tmp;
    }
}
