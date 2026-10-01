// s2x_servercmds.gsc - chat commands and an end-of-match map vote.
//
// Chat commands (replies go only to the player who asked):
//   !help  !rules  !discord  !nextmap  !vote      and !1 .. !N while a map vote is open
// Map vote: when the match ends, players pick the next map from the rotation. Choice 1 is always
// the rotation's own next map, so a vote for it, a tie or no votes leave the rotation alone; a
// winning choice 2..N replaces the next rotation entry. Needs an s2x.exe that reads s2x_nextmap
// (s2x_nextmap_api 1); on an older one the vote stays off and the chat commands still work.
//
// Installed by S2x Server Manager into s2x\scripts\mp\. Every MP server in the folder loads it;
// it stays idle unless server-<port>.cfg turns a feature on:
//   set s2x_chatcmds 1              !help !rules !discord !nextmap
//   set s2x_rules1 "..." .. s2x_rules5
//   set s2x_discord "..."
//   set s2x_mapvote 1               end-of-match vote; needs s2x.exe with s2x_nextmap_api >= 1
//   set s2x_mapvote_choices 3       2-5
//   set s2x_mapvote_time 15         10-30 s
// Test only (never written by the Manager):
//   set s2x_mapvote_test_bots 1     bots count as voters
//   set s2x_mapvote_test_vote k     every bot votes !k after the vote opens; 9 splits bots 2/3
//
// Voting: AIM moves the cursor up, FIRE moves it down, JUMP (or USE) votes for the row under it;
// the D-pad moves it too. !1 .. !N in chat still work. Keys come from notifyonplayercommand,
// the way stock S2 reads the D-pad and the forfeit button.
//
// HUD rule: settext gets only fixed labels, the N ballot rows and the result's map name, each
// set once per map (at most 9 + N distinct strings), and only in mv_text and mv_hud_result.
// Counts use setvalue and the clock settimer; nothing is printed in the middle of the screen,
// where it would sit on the panel. This file never notifies level "say" (that is
// s2x_server_events.gsc's job) and listens to the per-player "s2x_chat" notify only.

init()
{
	chat = getdvarint("s2x_chatcmds") == 1;
	vote = getdvarint("s2x_mapvote") == 1 && getdvarint("s2x_nextmap_api") >= 1
		&& getdvar("g_gametype") != "zombies";
	if (!chat && !vote)
		return;

	level.s2x_sc_chat = chat;
	level.s2x_sc_vote = vote;
	sc_names();
	sc_read_texts();
	level thread sc_watch_players();
	if (vote)
		level thread mv_main();
}

sc_log(text)
{
	println("[servercmds] " + text);
}

// ---------------------------------------------------------------------------
// Chat
// ---------------------------------------------------------------------------

sc_read_texts()
{
	level.s2x_sc_rules = [];
	for (i = 1; i <= 5; i++)
	{
		text = getdvar("s2x_rules" + i);
		if (text == "")
			break;
		level.s2x_sc_rules[level.s2x_sc_rules.size] = text;
	}
	level.s2x_sc_discord = getdvar("s2x_discord");
}

sc_watch_players()
{
	for (;;)
	{
		level waittill("connected", player);
		player thread sc_watch_chat();
	}
}

sc_watch_chat()
{
	self endon("disconnect");
	for (;;)
	{
		self waittill("s2x_chat", message, team_chat);
		// Handled in its own thread: a script error there cannot end this listener.
		self thread sc_handle(message);
	}
}

sc_handle(message)
{
	self endon("disconnect");
	if (message.size < 2 || getsubstr(message, 0, 1) != "!")
		return;
	words = strtok(tolower(message), " ");
	if (words.size == 0)
		return;
	cmd = words[0];

	if (isdefined(level.s2x_mv_state) && level.s2x_mv_state == "open")
	{
		choice = mv_parse_choice(words);
		if (choice > 0)
		{
			self mv_cast(choice);
			return;
		}
	}

	now = gettime();
	if (isdefined(self.s2x_sc_next) && now < self.s2x_sc_next)
		return;

	if (cmd == "!vote" && level.s2x_sc_vote)
	{
		self.s2x_sc_next = now + 2000;
		self mv_status();
		return;
	}
	if (!level.s2x_sc_chat)
		return;

	// Unknown commands get no reply, so other scripts can own their own "!" words.
	if (cmd == "!help")
		self sc_help();
	else if (cmd == "!rules")
		self sc_rules();
	else if (cmd == "!discord")
		self sc_discord();
	else if (cmd == "!nextmap")
		self sc_nextmap();
	else
		return;
	self.s2x_sc_next = now + 2000;
	sc_log(self.name + " " + cmd);
}

// Lists only what this server has: !rules and !discord need text from the host.
sc_help()
{
	text = "^3Server:^7";
	if (level.s2x_sc_rules.size > 0)
		text += "  !rules";
	if (level.s2x_sc_discord != "")
		text += "  !discord";
	text += "  !nextmap";
	if (level.s2x_sc_vote)
		text += "  !vote";
	self iprintln(text);
}

sc_rules()
{
	if (level.s2x_sc_rules.size == 0)
	{
		self iprintln("^3Server:^7 No rules are set.");
		return;
	}
	for (i = 0; i < level.s2x_sc_rules.size; i++)
	{
		self iprintln("^3Rule " + (i + 1) + ":^7 " + level.s2x_sc_rules[i]);
		wait 0.5;
	}
}

sc_discord()
{
	if (level.s2x_sc_discord == "")
		self iprintln("^3Server:^7 No Discord is set.");
	else
		self iprintln("^3Discord:^7 " + level.s2x_sc_discord);
}

sc_nextmap()
{
	next = sc_next_entry();
	if (!isdefined(next))
	{
		self iprintln("^3Next map:^7 follows the server rotation");
		return;
	}
	text = "^3Next map:^7 " + sc_entry_name(next[0], next[1]);
	if (next[2])
		text += " (set by the server)";
	else if (level.s2x_sc_vote)
		text += ", unless the map vote picks another";
	self iprintln(text);
}

// [map, gametype, fixed] of what plays next without a vote, or undefined.
sc_next_entry()
{
	preview = strtok(getdvar("s2x_nextmap_preview"), " ");
	if (preview.size >= 2)
		return sc_entry(preview[0], preview[1], preview.size >= 3 && preview[2] == "fixed");

	// Older s2x.exe: infer from the rotation when the current match is in it exactly once.
	pool = sc_rotation();
	here = -1;
	found = 0;
	for (i = 0; i < pool.size; i++)
	{
		if (pool[i][0] == getdvar("mapname") && pool[i][1] == getdvar("g_gametype"))
		{
			here = i;
			found++;
		}
	}
	if (found != 1)
		return undefined;
	next = pool[(here + 1) % pool.size];
	return sc_entry(next[0], next[1], 0);
}

sc_entry(map, gametype, fixed)
{
	entry = [];
	entry[0] = map;
	entry[1] = gametype;
	entry[2] = fixed;
	return entry;
}

// sv_maprotation as the Manager writes it ("gametype X map Y ..."), in the order the server
// walks it (already shuffled when shuffle-on-launch is on). Mirrors dedicated_party.cpp:447-529
// for well-formed input; it does not validate maps.
sc_rotation()
{
	pool = [];
	words = strtok(tolower(getdvar("sv_maprotation")), " ");
	gametype = getdvar("g_gametype");
	for (i = 0; i + 1 < words.size; i += 2)
	{
		if (words[i] == "gametype")
			gametype = words[i + 1];
		else if (words[i] == "map")
			pool[pool.size] = sc_entry(words[i + 1], gametype, 0);
	}
	return pool;
}

sc_entry_name(map, gametype)
{
	name = map;
	if (isdefined(level.s2x_sc_maps[map]))
		name = level.s2x_sc_maps[map];
	mode = gametype;
	if (isdefined(level.s2x_sc_modes[gametype]))
		mode = level.s2x_sc_modes[gametype];
	return name + " - " + mode;
}

// Mirrors tools/ServerManager/Models/GameData.cs Maps and Gametypes; a Manager test keeps them equal.
sc_names()
{
	m = [];
	m["mp_shipment_s2"] = "Shipment 1944";
	m["mp_d_day"] = "Pointe du Hoc";
	m["mp_aachen_v2"] = "Aachen";
	m["mp_carentan_s2"] = "Carentan";
	m["mp_carentan_s2_winter"] = "Winter Carentan";
	m["mp_canon_farm"] = "Gustav Cannon";
	m["mp_flak_tower"] = "Flak Tower";
	m["mp_forest_01"] = "Ardennes Forest";
	m["mp_london"] = "London Docks";
	m["mp_france_village"] = "Sainte Marie du Mont";
	m["mp_battleship_2"] = "USS Texas";
	m["mp_gibraltar_02"] = "Gibraltar";
	m["mp_sandbox_01"] = "Sandbox";
	m["mp_house"] = "Groesten Haus";
	m["mp_paris_s2"] = "Occupation";
	m["mp_prague"] = "Anthropoid";
	m["mp_wolfslair"] = "Valkyrie";
	m["mp_dunkirk"] = "Dunkirk";
	m["mp_egypt_02"] = "Egypt";
	m["mp_v2_rocket_02"] = "V2";
	m["mp_stalingrad"] = "Stalingrad";
	m["mp_market_garden"] = "Market Garden";
	m["mp_monte_cassino_v2"] = "Monte Cassino";
	m["mp_tank_graveyard_2"] = "Excavation";
	m["mp_airship"] = "Airship";
	m["mp_fuhrerbunker"] = "Chancellery";
	level.s2x_sc_maps = m;

	g = [];
	g["war"] = "Team Deathmatch";
	g["dom"] = "Domination";
	g["hp"] = "Hardpoint";
	g["dm"] = "Free-for-All";
	g["conf"] = "Kill Confirmed";
	g["sd"] = "Search and Destroy";
	g["ctf"] = "Capture the Flag";
	g["gun"] = "Gun Game";
	g["ball"] = "Gridiron";
	level.s2x_sc_modes = g;
}

// ---------------------------------------------------------------------------
// Map vote
// ---------------------------------------------------------------------------

mv_log(text)
{
	println("[mapvote] " + gettime() + " " + text);
}

mv_main()
{
	// game_win fires only on the final match end (1232:3160); game_ended fires every round.
	level waittill("game_win");
	if (!mv_should_run())
		return;
	if (!mv_build_ballot())
	{
		mv_log("skipped: fewer than two choices");
		return;
	}

	// Stock calls this synchronously after the outcome screen and the team-win final killcam,
	// before spawning_intermission and exitlevel (1232:3801-3802). Only ranked play sets it
	// (1288:10-16); keep and chain whatever is there.
	level.s2x_mv_prev_hook = level._id_75E7;
	level._id_75E7 = ::mv_hook;
	level.s2x_mv_state = "pending";

	// The Victory/Defeat screen covers the view until round_end_finished (1232:1945-1961).
	level thread mv_relay("round_end_finished", "s2x_mv_open");
	level thread mv_timeout("s2x_mv_open", 8);
	level waittill("s2x_mv_open");

	mv_open();
	mv_run();
}

mv_relay(source, event)
{
	level endon(event);
	level waittill(source);
	level notify(event);
}

mv_timeout(event, seconds)
{
	level endon(event);
	wait seconds;
	level notify(event);
}

mv_should_run()
{
	if (common_scripts\utility::_id_562E(level._id_3E16) || common_scripts\utility::_id_562E(level._id_4DFE))
	{
		mv_log("skipped: host-ended or forfeit");
		return 0;
	}
	if (maps\mp\_utility::_id_579B() && isdefined(game["switchedsides"]) && !game["switchedsides"])
	{
		mv_log("skipped: game two follows");
		return 0;
	}
	if (common_scripts\utility::_id_562E(level._id_7DD2))
		return 0;
	preview = strtok(getdvar("s2x_nextmap_preview"), " ");
	if (preview.size < 2)
		return 0;
	if (preview.size >= 3 && preview[2] == "fixed")
	{
		mv_log("skipped: the next map was set by the server");
		return 0;
	}
	if (mv_voters().size == 0)
	{
		mv_log("skipped: no human players");
		return 0;
	}
	return 1;
}

mv_is_voter(player)
{
	if (getdvarint("s2x_mapvote_test_bots") == 1)
		return 1;
	return !isbot(player) && !istestclient(player);
}

mv_voters()
{
	voters = [];
	foreach (player in level.players)
	{
		if (mv_is_voter(player))
			voters[voters.size] = player;
	}
	return voters;
}

// Choice 1 is always the rotation's own next entry, so a vote for it, a tie or no votes all
// leave the rotation alone. Choices 2..N: other rotation entries, a different map first.
// A winning choice 2..N replaces the next rotation entry natively (and skips the entry after it
// when that is the same match), so the rotation still moves one slot per match.
mv_build_ballot()
{
	count = getdvarint("s2x_mapvote_choices");
	if (count <= 0)
		count = 3;
	if (count < 2)
		count = 2;
	if (count > 5)
		count = 5;

	preview = strtok(getdvar("s2x_nextmap_preview"), " ");
	ballot = [];
	ballot[0] = sc_entry(preview[0], preview[1], 0);
	pool = sc_rotation();
	here_map = getdvar("mapname");
	here_mode = getdvar("g_gametype");

	for (pass = 0; pass < 2 && ballot.size < count; pass++)
	{
		candidates = [];
		foreach (entry in pool)
		{
			if (entry[0] == here_map && (pass == 0 || entry[1] == here_mode))
				continue;
			if (mv_on_ballot(ballot, entry, pass == 0) || mv_on_ballot(candidates, entry, pass == 0))
				continue;
			candidates[candidates.size] = entry;
		}
		while (ballot.size < count && candidates.size > 0)
		{
			pick = randomint(candidates.size);
			ballot[ballot.size] = candidates[pick];
			candidates = mv_without(candidates, pick);
		}
	}

	if (ballot.size < 2)
		return 0;
	level.s2x_mv_ballot = ballot;
	return 1;
}

mv_on_ballot(list, entry, by_map)
{
	foreach (other in list)
	{
		if (other[0] == entry[0] && (by_map || other[1] == entry[1]))
			return 1;
	}
	return 0;
}

mv_without(list, index)
{
	result = [];
	for (i = 0; i < list.size; i++)
	{
		if (i != index)
			result[result.size] = list[i];
	}
	return result;
}

mv_row_text(i)
{
	entry = level.s2x_mv_ballot[i];
	text = (i + 1) + ". " + sc_entry_name(entry[0], entry[1]);
	if (i == 0)
		text += " (next)";
	return text;
}

mv_open()
{
	seconds = getdvarint("s2x_mapvote_time");
	if (seconds <= 0)
		seconds = 15;
	if (seconds < 10)
		seconds = 10;
	if (seconds > 30)
		seconds = 30;

	level.s2x_mv_deadline = gettime() + seconds * 1000;
	level.s2x_mv_state = "open";
	mv_hud_create(seconds);

	// People get the cursor and the keys; in the bot test the bots do too, so that code runs.
	count = level.s2x_mv_ballot.size;
	testing = getdvarint("s2x_mapvote_test_bots") == 1;
	foreach (player in mv_voters())
	{
		if (testing || (!isbot(player) && !istestclient(player)))
			player mv_player_open();
	}
	mv_log("open: " + count + " choices, " + seconds + " s");

	if (getdvarint("s2x_mapvote_test_bots") == 1)
		level thread mv_test_votes();
}

mv_parse_choice(words)
{
	token = words[0];
	if ((token == "!vote" || token == "!v") && words.size >= 2)
		token = "!" + words[1];
	for (i = 1; i <= level.s2x_mv_ballot.size; i++)
	{
		if (token == "!" + i)
			return i;
	}
	return 0;
}

// A vote typed in chat: !k.
mv_cast(choice)
{
	if (!mv_is_voter(self))
		return;
	now = gettime();
	if (isdefined(self.s2x_mv_next) && now < self.s2x_mv_next)
		return;
	self.s2x_mv_next = now + 1000;
	self mv_pick(choice - 1);
}

// Records a vote (row index) and moves this player's bars onto it.
mv_pick(index)
{
	if (!mv_is_voter(self))
		return;
	self.s2x_mv_choice = index;
	self.s2x_mv_cursor = index;
	if (isdefined(self.s2x_mv_bar_pick))
	{
		self mv_place(self.s2x_mv_bar_pick, index);
		self.s2x_mv_bar_pick.alpha = 0.45;
	}
	if (isdefined(self.s2x_mv_bar_cursor))
		self mv_place(self.s2x_mv_bar_cursor, index);
	mv_log(self.name + " voted " + (index + 1));
}

// ---------------------------------------------------------------------------
// Keys. notifyonplayercommand registrations last for the map, which ends with the vote.
// ---------------------------------------------------------------------------

mv_player_open()
{
	self.s2x_mv_cursor = 0;
	self.s2x_mv_bar_cursor = self mv_bar((1, 1, 1), 0.16);
	self.s2x_mv_bar_pick = self mv_bar((0.3, 0.9, 0.4), 0);
	self mv_place(self.s2x_mv_bar_cursor, 0);
	self mv_place(self.s2x_mv_bar_pick, 0);

	self notifyonplayercommand("s2x_mv_up", "+speed_throw");
	self notifyonplayercommand("s2x_mv_up", "+toggleads_throw");
	self notifyonplayercommand("s2x_mv_up", "+actionslot 1");
	self notifyonplayercommand("s2x_mv_down", "+attack");
	self notifyonplayercommand("s2x_mv_down", "+actionslot 2");
	self notifyonplayercommand("s2x_mv_pick", "+gostand");
	self notifyonplayercommand("s2x_mv_pick", "+usereload");
	self notifyonplayercommand("s2x_mv_pick", "+activate");
	self thread mv_key("s2x_mv_up", -1);
	self thread mv_key("s2x_mv_down", 1);
	self thread mv_key("s2x_mv_pick", 0);
}

mv_key(event, step)
{
	self endon("disconnect");
	level endon("s2x_mv_closed");
	for (;;)
	{
		self waittill(event);
		if (level.s2x_mv_state != "open")
			continue;
		now = gettime();
		if (isdefined(self.s2x_mv_key_next) && now < self.s2x_mv_key_next)
			continue;
		self.s2x_mv_key_next = now + 150;
		if (step == 0)
		{
			self mv_pick(self.s2x_mv_cursor);
			continue;
		}
		count = level.s2x_mv_ballot.size;
		self.s2x_mv_cursor = (self.s2x_mv_cursor + step + count) % count;
		self mv_place(self.s2x_mv_bar_cursor, self.s2x_mv_cursor);
	}
}

mv_player_close()
{
	if (isdefined(self.s2x_mv_bar_cursor))
		self.s2x_mv_bar_cursor destroy();
	if (isdefined(self.s2x_mv_bar_pick))
		self.s2x_mv_bar_pick destroy();
	self.s2x_mv_bar_cursor = undefined;
	self.s2x_mv_bar_pick = undefined;
}

mv_status()
{
	if (!isdefined(level.s2x_mv_state) || level.s2x_mv_state != "open")
	{
		self iprintln("^3Map vote:^7 opens when the match ends");
		return;
	}
	for (i = 0; i < level.s2x_mv_ballot.size; i++)
		self iprintln("^3!" + (i + 1) + "^7 " + mv_row_text(i));
}

mv_tally()
{
	counts = [];
	for (i = 0; i < level.s2x_mv_ballot.size; i++)
		counts[i] = 0;
	foreach (player in mv_voters())
	{
		if (isdefined(player.s2x_mv_choice))
			counts[player.s2x_mv_choice]++;
	}
	return counts;
}

mv_all_voted()
{
	voters = mv_voters();
	if (voters.size == 0)
		return 0;
	foreach (player in voters)
	{
		if (!isdefined(player.s2x_mv_choice))
			return 0;
	}
	return 1;
}

// Index of the single most-voted choice, or -1 for no votes or a tie.
mv_winner(counts)
{
	best = -1;
	best_count = 0;
	tie = 0;
	for (i = 0; i < counts.size; i++)
	{
		if (counts[i] > best_count)
		{
			best = i;
			best_count = counts[i];
			tie = 0;
		}
		else if (counts[i] == best_count && best_count > 0)
			tie = 1;
	}
	if (tie)
		return -1;
	return best;
}

mv_run()
{
	for (;;)
	{
		mv_hud_update(mv_tally());
		now = gettime();
		if (now >= level.s2x_mv_deadline)
		{
			// A final killcam still playing (FFA and ties are not waited for, 1232:3791) may finish.
			if (!common_scripts\utility::_id_562E(level._id_8C03) || now >= level.s2x_mv_deadline + 10000)
				break;
		}
		else if (mv_all_voted() && level.s2x_mv_deadline > now + 3000)
		{
			level.s2x_mv_deadline = now + 3000;
			level.s2x_mv_timer settimer(3);
		}
		wait 0.5;
	}
	mv_close(mv_tally());
}

mv_close(counts)
{
	level.s2x_mv_state = "closed";
	level notify("s2x_mv_closed");
	winner = mv_winner(counts);
	shown = winner;
	if (shown < 0)
		shown = 0;
	entry = level.s2x_mv_ballot[shown];

	// Choice 1 is the rotation's own next entry: nothing to hand over.
	if (winner > 0)
		setdvar("s2x_nextmap", entry[0] + " " + entry[1]);

	mv_hud_result(winner, shown);
	mv_log("closed: winner " + (winner + 1) + ", next " + entry[0] + " " + entry[1]);

	wait 3;
	mv_hud_destroy();
	level.s2x_mv_state = "done";
}

// Runs inside the endGame thread. No endon, nothing that can error: an error here ends endGame
// before exitlevel, and the dedicated party's match_running stage never times out
// (dedicated_party.cpp:997-1004). All vote work happens in mv_main's thread.
mv_hook()
{
	// If stock gets here before the outcome screen closed, wait (bounded) for the vote to open.
	start = gettime();
	while (level.s2x_mv_state == "pending" && gettime() < start + 10000)
		wait 0.25;
	if (isdefined(level.s2x_mv_deadline))
	{
		level.s2x_mv_in_hook = 1;
		level thread mv_watchdog();
		cap = level.s2x_mv_deadline + 14000;
		while (level.s2x_mv_state != "done" && gettime() < cap)
			wait 0.25;
		level.s2x_mv_in_hook = 0;
	}
	if (isdefined(level.s2x_mv_prev_hook))
		[[ level.s2x_mv_prev_hook ]]();
}

// Last resort: endGame died inside the hook, so leave the level from here.
mv_watchdog()
{
	level endon("exitLevel_called");
	wait 60;
	if (common_scripts\utility::_id_562E(level.s2x_mv_in_hook))
	{
		mv_log("endGame stopped inside the vote hook; calling exitlevel");
		exitlevel(0);
	}
}

mv_test_votes()
{
	wait 2;
	choice = getdvarint("s2x_mapvote_test_vote");
	if (choice <= 0)
		return;
	n = 0;
	foreach (player in level.players)
	{
		if (!isbot(player) && !istestclient(player))
			continue;
		pick = choice;
		if (choice == 9)
			pick = 2 + n % 2;
		n++;
		player notify("s2x_chat", "!" + pick, 0);
	}
}

// ---------------------------------------------------------------------------
// Vote HUD. The panel is made of level elements, so spectators and late joiners see it too;
// each voter also has two bars of their own, the cursor and their vote.
// Layout (virtual 640x480, offsets from the screen centre):
//   backdrop (black) with a gold title strip, MAP VOTE, two hint lines, one row per choice
//   with its count on the right, "Vote ends in" and the clock.
// Materials: "black" and the stock HUD bar's "progress_bar_fill" (tinted), both always loaded.
// ---------------------------------------------------------------------------

mv_text(text, point, x, y, scale)
{
	elem = maps\mp\gametypes\_hud_util::_id_2829("default", scale);
	elem maps\mp\gametypes\_hud_util::setpoint(point, "CENTER", x, y);
	elem settext(text);
	mv_style(elem);
	level.s2x_mv_elems[level.s2x_mv_elems.size] = elem;
	return elem;
}

mv_style(elem)
{
	elem.archived = 0;
	elem.foreground = 1;
	elem.hidewheninmenu = 0;
	elem.sort = 10;
}

mv_box(material, width, height, y, color, alpha, sort)
{
	elem = maps\mp\gametypes\_hud_util::_id_282A(material, width, height);
	elem maps\mp\gametypes\_hud_util::setpoint("CENTER", "CENTER", 0, y);
	mv_style(elem);
	elem.color = color;
	elem.alpha = alpha;
	elem.sort = sort;
	level.s2x_mv_elems[level.s2x_mv_elems.size] = elem;
	return elem;
}

// A per-player bar behind one row (see mv_place).
mv_bar(color, alpha)
{
	bar = self maps\mp\gametypes\_hud_util::createicon("progress_bar_fill", level.s2x_mv_width - 12, 18);
	mv_style(bar);
	bar.color = color;
	bar.alpha = alpha;
	bar.sort = 9;
	return bar;
}

mv_place(bar, row)
{
	if (isdefined(bar))
		bar maps\mp\gametypes\_hud_util::setpoint("CENTER", "CENTER", 0, level.s2x_mv_row_y[row]);
}

mv_hud_create(seconds)
{
	level.s2x_mv_elems = [];
	count = level.s2x_mv_ballot.size;
	width = 360;
	height = 92 + count * 20;
	centre = -30;
	top = centre - height / 2;
	left = 0 - width / 2 + 16;
	right = width / 2 - 16;
	level.s2x_mv_width = width;
	level.s2x_mv_centre = centre;

	level.s2x_mv_back = mv_box("black", width, height, centre, (1, 1, 1), 0.78, 7);
	level.s2x_mv_strip = mv_box("progress_bar_fill", width, 28, top + 14, (0.55, 0.42, 0.12), 0.9, 8);
	level.s2x_mv_title = mv_text("MAP VOTE", "CENTER", 0, top + 14, 1.6);
	level.s2x_mv_title.color = (1, 0.92, 0.7);
	level.s2x_mv_hint = mv_text("AIM / FIRE to move, JUMP to vote", "CENTER", 0, top + 38, 1.1);
	level.s2x_mv_hint2 = mv_text("or type !1 - !" + count + " in chat", "CENTER", 0, top + 52, 0.9);
	level.s2x_mv_hint2.alpha = 0.7;

	level.s2x_mv_rows = [];
	level.s2x_mv_counts = [];
	level.s2x_mv_row_y = [];
	for (i = 0; i < count; i++)
	{
		y = top + 74 + i * 20;
		level.s2x_mv_row_y[i] = y;
		level.s2x_mv_rows[i] = mv_text(mv_row_text(i), "LEFT", left, y, 1.3);

		value = maps\mp\gametypes\_hud_util::_id_2829("default", 1.3);
		value maps\mp\gametypes\_hud_util::setpoint("RIGHT", "CENTER", right, y);
		value setvalue(0);
		mv_style(value);
		value.color = (1, 0.85, 0.3);
		level.s2x_mv_elems[level.s2x_mv_elems.size] = value;
		level.s2x_mv_counts[i] = value;
	}

	y = top + 80 + count * 20;
	level.s2x_mv_ends = mv_text("Vote ends in", "RIGHT", 10, y, 1.1);
	timer = maps\mp\gametypes\_hud_util::_id_282B("default", 1.1);
	timer maps\mp\gametypes\_hud_util::setpoint("LEFT", "CENTER", 16, y);
	timer settimer(seconds);
	mv_style(timer);
	level.s2x_mv_elems[level.s2x_mv_elems.size] = timer;
	level.s2x_mv_timer = timer;
	level.s2x_mv_shown = [];
}

mv_hud_update(counts)
{
	best = mv_winner(counts);
	for (i = 0; i < counts.size; i++)
	{
		if (!isdefined(level.s2x_mv_shown[i]) || level.s2x_mv_shown[i] != counts[i])
		{
			level.s2x_mv_counts[i] setvalue(counts[i]);
			level.s2x_mv_shown[i] = counts[i];
		}
		if (i == best)
			level.s2x_mv_rows[i].color = (1, 0.85, 0.3);
		else
			level.s2x_mv_rows[i].color = (1, 1, 1);
	}
}

// The result replaces the ballot inside a smaller panel: NEXT MAP on the strip, the map on
// its own line under it, and one line saying how it was chosen.
mv_hud_result(winner, shown)
{
	foreach (player in level.players)
		player mv_player_close();

	foreach (row in level.s2x_mv_rows)
		row.alpha = 0;
	foreach (value in level.s2x_mv_counts)
		value.alpha = 0;
	level.s2x_mv_hint.alpha = 0;
	level.s2x_mv_hint2.alpha = 0;
	level.s2x_mv_ends.alpha = 0;
	level.s2x_mv_timer.alpha = 0;

	width = level.s2x_mv_width;
	height = 96;
	centre = level.s2x_mv_centre;
	top = centre - height / 2;
	level.s2x_mv_back setshader("black", width, height);
	level.s2x_mv_back maps\mp\gametypes\_hud_util::setpoint("CENTER", "CENTER", 0, centre);
	level.s2x_mv_strip maps\mp\gametypes\_hud_util::setpoint("CENTER", "CENTER", 0, top + 14);
	level.s2x_mv_title maps\mp\gametypes\_hud_util::setpoint("CENTER", "CENTER", 0, top + 14);
	level.s2x_mv_title settext("NEXT MAP");

	entry = level.s2x_mv_ballot[shown];
	name = mv_text(sc_entry_name(entry[0], entry[1]), "CENTER", 0, top + 48, 1.5);
	name.color = (0.55, 1, 0.55);
	if (winner < 0)
		how = mv_text("No winner: the rotation continues", "CENTER", 0, top + 74, 1.0);
	else
		how = mv_text("Chosen by vote", "CENTER", 0, top + 74, 1.0);
	how.alpha = 0.75;
}

mv_hud_destroy()
{
	foreach (player in level.players)
		player mv_player_close();
	foreach (elem in level.s2x_mv_elems)
	{
		if (isdefined(elem))
			elem destroy();
	}
	level.s2x_mv_elems = [];
}
