static bool DoAutoGVAR( ReDefine::ScriptCode& code, const std::vector<std::string>& values )
{
    if( !code.IsValues( __FUNCTION__, values, 1 ) )
        return false;

    unsigned int idx;
    if( !code.GetINDEX( __FUNCTION__, values[0], idx ) )
        return false;

    if( code.Arguments[idx].length() > 5 /* GVAR_ */ && code.Arguments[idx].substr( 0, 5 ) != "GVAR_" )
    {
        int         val;
        std::string prefixed = "GVAR_" + code.Arguments[idx];

        if( code.Parent->GetDefineValue( "GVAR", prefixed, val ) )
            code.Arguments[idx] = prefixed;
    }

    return true;
}
