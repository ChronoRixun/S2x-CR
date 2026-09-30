// Development observer only. Do not ship with the server addon.
// Requires an actual human client; deliberately never registers bots as recipients.
init()
{
    if (!getdvarint("s2x_server_admin_ready")) return;
    level thread observe_connections();
    if (isdefined(level.players)) foreach(player in level.players) player thread observe_notices();
}
observe_connections()
{
    level endon("game_ended");
    for (;;) { level waittill("connected", player); player thread observe_notices(); }
}
observe_notices()
{
    self endon("disconnect");
    level endon("game_ended");
    if (isbot(self) || isdefined(self.admin_probe_started)) return;
    self.admin_probe_started = 1;
    self.admin_probe_sequence = 0;
    for (;;)
    {
        self waittill("s2x_admin_notice", message, warning);
        self.admin_probe_sequence++;
        // The helper prints with iprintlnbold, which leaves no server-side HUD state to inspect.
        println("ADMIN PROBE received slot=" + self getentitynumber() + " warning=" + warning + " sequence=" + self.admin_probe_sequence + " text=" + message);
    }
}
