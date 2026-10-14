// Temporary dedicated-server smoke test; do not ship as an enabled game script.
main()
{
    level thread watch_connections();
    level thread watch_chat();
}

init()
{
    count = 0;
    previous = fileread( "issue9-validation-counter.txt" );
    if ( isdefined( previous ) )
        count = int( previous );
    count++;
    filewrite( "issue9-validation-counter.txt", "" + count );
    filewrite( "issue9-validation-empty.txt", "" );
    if ( fileread( "issue9-validation-counter.txt" ) == "" + count &&
        fileread( "issue9-validation-empty.txt" ) == "" &&
        !isdefined( fileread( "issue9-validation-missing.txt" ) ) )
        println( "ISSUE9 PASS: storage round-trip, empty/missing, map count " + count );
    else
        println( "ISSUE9 FAIL: storage" );
}

watch_connections()
{
    for ( ;; )
    {
        level waittill( "connected", player );
        player thread check_player();
    }
}

check_player()
{
    self endon( "disconnect" );
    self waittill( "spawned_player" );
    wait 1;
    if ( isbot( self ) && getip( self ) == "" )
        println( "ISSUE9 PASS: bot has no IP" );
    else
        println( "ISSUE9 FAIL: unexpected test player" );
    // Exercises the shipped player-to-level bridge, not the C++ command hook.
    self notify( "s2x_chat", "script bridge fixture", 1 );
}

watch_chat()
{
    for ( ;; )
    {
        level waittill( "say", player, message, team_chat );
        if ( isdefined( player ) && message == "script bridge fixture" && team_chat == 1 )
            println( "ISSUE9 PASS: chat bridge preserves player, text and team flag" );
        else
            println( "ISSUE9 FAIL: chat bridge" );
    }
}
