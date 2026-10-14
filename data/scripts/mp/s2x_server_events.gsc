// Bridge native per-player chat notifications to a convenient level event.
// Scripts may wait on level: waittill("say", player, message, team_chat).
main()
{
    level thread watch_players();
}

// Optional local snapshot for tools/server-status.ps1 -RosterFile. Nothing is
// published unless the server owner chooses a plain filename in the config.
init()
{
    filename = getdvar( "s2x_roster_file" );
    if ( filename != "" )
        level thread publish_roster( filename );
}

publish_roster( filename )
{
    level endon( "game_ended" );
    for ( ;; )
    {
        wait 5;
        filewrite( filename, getplayerroster() );
    }
}

watch_players()
{
    for ( ;; )
    {
        level waittill( "connected", player );
        player thread watch_chat();
    }
}

watch_chat()
{
    self endon( "disconnect" );
    for ( ;; )
    {
        self waittill( "s2x_chat", message, team_chat );
        level notify( "say", self, message, team_chat );
    }
}
