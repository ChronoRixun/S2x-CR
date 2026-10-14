// Gun Game replaces the generated bot costume with player-profile costume data
// after each weapon change. Bots have no usable profile costume. Restore the
// same division costume source used for bots by maps/mp/gametypes/_class.
main()
{
    if ( getdvar( "g_gametype" ) != "gun" || getdvar( "mapname" ) == "mp_sandbox_01" )
        return;

    level thread watch_connections();
}

watch_connections()
{
    for ( ;; )
    {
        level waittill( "connected", player );
        if ( isbot( player ) )
            player thread repair_loadouts();
    }
}

repair_loadouts()
{
    self endon( "disconnect" );
    self.s2x_gungame_costumes = [];

    for ( ;; )
    {
        self waittill( "applyLoadout" );
        if ( isdefined( self._id_5097 ) && self._id_5097 )
            continue;
        // _id_0079 is the current division. Gun Game is free-for-all and its
        // stock outfit path selects Allies. Division 5 uses the default outfit.
        division = 0;
        if ( isdefined( self._id_0079 ) && self._id_0079 != 5 )
            division = self._id_0079;

        if ( !isdefined( self.s2x_gungame_costumes[division] ) )
            self.s2x_gungame_costumes[division] = _func_333( division, 1 );

        if ( !isdefined( self.s2x_gungame_costumes[division] ) || self.s2x_gungame_costumes[division].size == 0 )
            continue;

        // _id_267E is the costume array. _meth_84C7 applies customization,
        // matching the stock teams::_id_73CA path used immediately before
        // Gun Game sends applyLoadout. Keep the outfit stable within a division.
        self._id_267E = self.s2x_gungame_costumes[division];
        self _meth_84C7( self._id_267E, undefined, 1, 1 );
        self loadcustomizationplayerview( self );
        if ( getdvarint( "s2x_gungame_debug" ) )
            println( "S2x Gun Game: repaired bot costume for division " + division );
    }
}
