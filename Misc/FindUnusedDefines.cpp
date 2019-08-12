void FindUnusedDefines( ReDefine* redefine, const std::string& type, const std::string& counter )
{
    auto itDefines = redefine->RegularDefines.find( type );
    if( itDefines == redefine->RegularDefines.end() )
    {
        redefine->WARNING( __FUNCTION__, "unknown define type<%s>", type.c_str() );
        return;
    }

    auto itCounter = redefine->Status.Process.Counters.find( counter );
    if( itCounter == redefine->Status.Process.Counters.end() )
    {
        redefine->WARNING( __FUNCTION__, "unknown counter<%s>", counter.c_str() );
        return;
    }

    auto itCounterEnd = itCounter->second.end();

    for( const auto& itValue : itDefines->second )
    {
        const std::string& name = itValue.second;

        if( itCounter->second.find( name ) == itCounterEnd )
        {
            redefine->WARNING( __FUNCTION__, "%s (%d)", name.c_str(), itValue.first );
        }
    }
}
