// Optional example, not enabled by default. Install on the server as
// s2x/scripts/mp/scripting_api.gsc alongside s2x_server_events.gsc.
init()
{
    level thread watch_connections();
    level thread watch_commands();
}

watch_connections()
{
    for ( ;; )
    {
        level waittill( "connected", player );
        if ( !isbot( player ) && !istestclient( player ) )
            player thread record_visit();
    }
}

record_visit()
{
    self endon( "disconnect" );
    self waittill( "spawned_player" );
    guid = self getguid();
    filename = "visits-" + guid + ".txt";
    previous = fileread( filename );
    self.s2x_visits = 1;
    if ( isdefined( previous ) && previous != "" )
        self.s2x_visits = int( previous ) + 1;
    filewrite( filename, "" + self.s2x_visits );
    self iPrintLn( "^3Welcome! ^7Type !visits to see your saved map visits." );
}

watch_commands()
{
    for ( ;; )
    {
        level waittill( "say", player, message, team_chat );
        if ( message == "!visits" && isdefined( player.s2x_visits ) )
            player iPrintLn( "^3Server: ^7Saved map visits: " + player.s2x_visits );
    }
}
