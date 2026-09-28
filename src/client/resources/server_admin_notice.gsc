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
        self thread show_notice(message, warning);
    }
}
show_notice(message, warning)
{
    self notify("s2x_admin_notice_replace");
    self endon("s2x_admin_notice_replace");
    self endon("disconnect");
    level endon("game_ended");
    if (!isdefined(self.s2x_admin_notice_hud))
    {
        hud = newclienthudelem(self);
        hud.alignx = "center";
        hud.aligny = "top";
        hud._id_00C6 = "center";
        hud._id_01CA = "top";
        hud.x = 0;
        hud.y = 60;
        hud.fontscale = 1.2;
        hud.foreground = 1;
        hud.sort = 35;
        hud.archived = 0;
        self.s2x_admin_notice_hud = hud;
    }
    hud = self.s2x_admin_notice_hud;
    title = "SERVER NOTICE";
    hud.color = (0.85, 1, 0.85);
    if (warning) { title = "SERVER WARNING"; hud.color = (1, 0.72, 0.25); }
    hud settext(title + "\n" + message);
    hud.alpha = 1;
    wait 10;
    if (isdefined(hud)) hud.alpha = 0;
}
cleanup_notice(player, slot)
{
    common_scripts\utility::_id_A70E(level, "game_ended", player, "disconnect");
    serveradminnoticeready(slot, 0);
    if (isdefined(player))
    {
        player.s2x_admin_notice_listening = 0;
        if (isdefined(player.s2x_admin_notice_hud))
        {
            player.s2x_admin_notice_hud destroy();
            player.s2x_admin_notice_hud = undefined;
        }
    }
}
