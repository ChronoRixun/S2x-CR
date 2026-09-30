// Local-owner Server Manager notices. Inert without the authenticated dedicated bridge.
init()
{
    if (!getdvarint("s2x_server_admin_ready")) return;
    level thread start_notices();
}
start_notices()
{
    level endon("game_ended");
    // Native map-init reset must finish before advertising script availability.
    waitframe();
    level thread watch_connections();
    if (isdefined(level.players))
        foreach (player in level.players) player thread watch_notices();
    serveradminnoticeready(-1, 1);
    level waittill("game_ended");
}
watch_connections()
{
    level thread clear_map_ready();
    level endon("game_ended");
    for (;;)
    {
        level waittill("connected", player);
        player thread watch_notices();
    }
}
clear_map_ready()
{
    level waittill("game_ended");
    serveradminnoticeready(-1, 0);
}
watch_notices()
{
    self endon("disconnect");
    level endon("game_ended");
    if (isbot(self) || (isdefined(self.s2x_admin_notice_listening) && self.s2x_admin_notice_listening)) return;
    self.s2x_admin_notice_listening = 1;
    slot = self getentitynumber();
    level thread cleanup_notice(self, slot);
    // Connected can precede native active-client state; wait without losing cleanup.
    while (!serveradminnoticeready(slot, 1)) wait 0.1;
    for (;;)
    {
        self waittill("s2x_admin_notice", message, warning);
        // Bold print carries the text in the server command itself. Never settext here:
        // each distinct HUD string permanently takes an engine string slot for the map,
        // and running out of slots crashes the server.
        if (warning) self iprintlnbold("^3SERVER WARNING:^7 " + message);
        else self iprintlnbold("^2SERVER NOTICE:^7 " + message);
    }
}
cleanup_notice(player, slot)
{
    common_scripts\utility::_id_A70E(level, "game_ended", player, "disconnect");
    serveradminnoticeready(slot, 0);
    if (isdefined(player)) player.s2x_admin_notice_listening = 0;
}
