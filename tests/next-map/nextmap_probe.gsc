// Test fixture for tests/next-map (s2x_nextmap). Bots-only private LAN server; never ship it
// as an enabled game script. Copy into <server>/s2x/scripts/mp/ for a run, remove afterwards.
// Idle unless scr_nmtest is set:
//   scr_nmtest 1   print the next-map dvars at each map start
//   scr_nmtest 2   also run the B2 rejection steps: once per match, set s2x_nextmap to the next
//                  value in the list below (scr_nmtest_step counts across matches)
//   scr_nmtest_b11 1   print the B11 cfg-quoting dvars (length and value) and the rotation pool
//                      size parsed the way s2x_servercmds.gsc parses it
init()
{
	if (getdvarint("scr_nmtest") < 1)
		return;

	println("[nmtest] start map=" + getdvar("mapname") + " gt=" + getdvar("g_gametype")
		+ " api=" + getdvarint("s2x_nextmap_api")
		+ " preview=\"" + getdvar("s2x_nextmap_preview") + "\""
		+ " nextmap=\"" + getdvar("s2x_nextmap") + "\"");

	if (getdvarint("scr_nmtest_b11") == 1)
		b11_report();

	level thread b2_steps();

	// B7: where the time goes between endMatch and the map exit.
	level thread b7_watch("game_ended");
	level thread b7_watch("game_win");
	level thread b7_watch("round_end_finished");
	level thread b7_watch("final_killcam_done");
	level thread b7_watch("spawning_intermission");
	level thread b7_watch("exitLevel_called");

	if (getdvarint("scr_nmtest_hb") == 1)
		level thread b7_heartbeat();
}

// Shows whether the game keeps running after endMatch (one line per second).
b7_heartbeat()
{
	for (;;)
	{
		wait 1;
		println("[nmtest] hb t=" + gettime() + " players=" + level.players.size);
	}
}

b7_watch(name)
{
	level waittill(name);
	println("[nmtest] b7 " + name + " t=" + gettime());
}

b2_steps()
{
	level endon("game_ended");

	// scr_nmtest can be raised to 2 mid-match from the console.
	while (getdvarint("scr_nmtest") != 2)
		wait 0.5;

	values = [];
	values[values.size] = "mp_shipment_s2 war;quit";
	values[values.size] = "mp_doesnotexist war";
	values[values.size] = "mp_shipment_s2 war extra";
	values[values.size] = "MP_SHIPMENT_S2 DM";
	values[values.size] = "mp_shipment_s2";
	values[values.size] = "  mp_carentan_s2   dom  ";

	step = getdvarint("scr_nmtest_step");
	if (step >= values.size)
	{
		println("[nmtest] b2 done, nothing set");
		return;
	}

	setdvar("s2x_nextmap", values[step]);
	setdvar("scr_nmtest_step", "" + (step + 1));
	println("[nmtest] b2 step " + step + " set s2x_nextmap \"" + values[step] + "\" read back \""
		+ getdvar("s2x_nextmap") + "\"");
}

b11_report()
{
	names = [];
	names[names.size] = "nm_semi";
	names[names.size] = "nm_semi_marker";
	names[names.size] = "nm_url";
	names[names.size] = "nm_comment";
	names[names.size] = "nm_trailc";
	names[names.size] = "nm_bslash_mid";
	names[names.size] = "nm_bslash_end";
	names[names.size] = "nm_after_bslash";
	names[names.size] = "nm_colour";
	names[names.size] = "nm_caret_end";
	names[names.size] = "nm_pct";
	names[names.size] = "nm_utf8";
	names[names.size] = "nm_apos";
	names[names.size] = "nm_long120";
	names[names.size] = "nm_unquoted";
	names[names.size] = "nm_pct_s";
	names[names.size] = "nm_last";

	for (i = 0; i < names.size; i++)
	{
		value = getdvar(names[i]);
		println("[nmtest] b11 " + names[i] + " len=" + value.size + " value=<" + value + ">");
	}

	rotation = getdvar("sv_maprotation");
	words = strtok(tolower(rotation), " ");
	pool = 0;
	for (i = 0; i + 1 < words.size; i += 2)
	{
		if (words[i] == "map")
			pool++;
	}

	println("[nmtest] b11 sv_maprotation len=" + rotation.size + " words=" + words.size + " pool=" + pool);
}
