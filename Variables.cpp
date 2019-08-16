#include "FOClassic/Ini.h"

#include "ReDefine.h"

void ReDefine::FinishVariables()
{
    VariablesPrototypes.clear();
    VariablesGuessing.clear();
}

//

bool ReDefine::ReadConfigVariables( const std::string& section )
{
    FinishVariables();

    // [VariableGuess]
    // else if( section == sectionPrefix + "Guess" )
    // {
    //    std::vector<std::string> types = Config->GetStrVec( section, section );
    //    if( !types.size() )
    //        continue;
    //
    //    // type validation is part of ProcessHeaders(),
    //    // as at this point *Defines maps might not be initialized yet,
    //    VariablesGuessing = types;

    // [Variable] //

    if( !Config->IsSection( section ) )
        return true;

    std::vector<std::string> keys;
    if( !Config->GetSectionKeys( section, keys ) )
        return true;

    for( const auto& variable : keys )
    {
        if( Config->GetStrVec( section, variable ).size() != 1 )
            continue; // TODO warning

        // type validation is part of ProcessHeaders(),
        // as at this point *Defines maps might not be initialized yet
        VariablesPrototypes[variable] = Config->GetStr( section, variable );
    }

    return true;
}
