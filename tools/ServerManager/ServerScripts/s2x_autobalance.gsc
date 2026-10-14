// s2x_autobalance.gsc - keeps a match at the server's bot fill size and its teams even.
//
// Shipped inside S2x Server Manager, which installs it into the game folder and turns it on
// per server by writing these dvars into server-<port>.cfg:
//   set s2x_autobalance 1            on; anything else leaves this script idle
//   set s2x_autobalance_target 12    match size: 12 is 6v6, 18 is 9v9
//   set bot_fill 0                   this script owns the bots, not the one-shot native fill
// Optional:
//   set s2x_autobalance_delay 15     countdown before humans are moved, in seconds
//   set s2x_autobalance_live 1       move a live human straight away (0: at their next death)
//
// Rules:
// - Bots = target - humans, never below zero. A human joining replaces a bot; a human
//   leaving gets one back. Humans are never kicked and never turned away.
// - Bots go to the short side, so team totals stay even.
// - Humans are kept even too. When one side has two or more extra humans, everyone sees a
//   countdown, then random humans from the bigger side switch. A live player keeps their
//   streak and switches on the spot; anyone the swap is unsafe for (riding or placing a
//   streak, carrying an objective, in the air) switches when they next die.
// - One-life modes (Search and Destroy) only change at a round boundary or with dead bots.
// - Free-for-all modes only keep the player count at the target.
//
// Never settext dynamic text here: each distinct HUD string holds an engine string slot for
// the rest of the map and running out crashes the server. Dynamic text goes through
// iprintlnbold; the countdown uses one fixed label and a numeric timer.

init()
{
	if (getdvarint("s2x_autobalance") != 1)
		return;

	gametype = getdvar("g_gametype");
	if (ab_team_mode(gametype))
		level.s2x_ab_teams = 1;
	else if (gametype == "dm" || gametype == "gun")
		level.s2x_ab_teams = 0;
	else
		return;

	// The native fill decides at level start and then re-reads bot_fill every half second,
	// so zeroing it here, before any wait, stops it adding bots behind this script.
	if (getdvarint("s2x_autobalance_target") <= 0 && getdvarint("bot_fill") > 0)
		setdvar("s2x_autobalance_target", getdvarint("bot_fill"));
	setdvar("bot_fill", 0);

	// Stock autobalance counts bots as players and calls a move function S2 never defines.
	setdvar("scr_teambalance", 0);

	level thread ab_main();
	if (level.s2x_ab_teams)
		level thread ab_round_boundary();
}

ab_team_mode(gametype)
{
	return gametype == "war" || gametype == "dom" || gametype == "hp" || gametype == "conf"
		|| gametype == "sd" || gametype == "ctf" || gametype == "ball";
}

ab_log(text)
{
	println("[autobalance] " + text);
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

ab_main()
{
	level endon("game_ended");

	while (!isdefined(level.players) || !isdefined(level.bot_funcs) || !isdefined(level.bot_funcs["bots_spawn"]))
		wait 0.5;

	// _bots::init decides whether to start its own population monitor after one second.
	wait 2;
	level notify("bot_connect_monitor");

	level.s2x_ab_counter = 0;
	level.s2x_ab_counting_down = 0;
	level.s2x_ab_seen = 0;
	level.s2x_ab_adding = 0;
	level.s2x_ab_round_start = gettime();
	level.s2x_ab_last_status = "";
	level thread ab_prematch_then_hud();

	// After a round restart every client connects again; wait until the list stops changing
	// so the first count does not miss bots that are still coming back.
	last = -1;
	for (steady = 0; steady < 3 && gettime() - level.s2x_ab_round_start < 15000; )
	{
		if (level.players.size == last)
			steady++;
		else
			steady = 0;
		last = level.players.size;
		wait 1;
	}

	if (level.s2x_ab_teams)
		ab_log("on, target " + ab_target());
	else
		ab_log("on, target " + ab_target() + " (free-for-all: player count only)");

	for (;;)
	{
		if (game["state"] == "playing")
		{
			c = ab_count();
			ab_debug_status(c);
			if (!ab_population(c) && level.s2x_ab_teams && isdefined(level.s2x_ab_prematch_done))
				ab_humans(c);
		}

		wait 1;
	}
}

// set s2x_autobalance_debug 1 logs the counts whenever they change.
ab_debug_status(c)
{
	if (getdvarint("s2x_autobalance_debug") != 1)
		return;
	if (level.s2x_ab_teams)
		status = "allies " + c.humans["allies"].size + "h+" + c.bots["allies"].size + "b, axis " + c.humans["axis"].size + "h+" + c.bots["axis"].size + "b";
	else
		status = "players " + c.in_game + "h+" + c.all_bots.size + "b";
	status = status + ", joining " + c.humans_joining + "h+" + c.bots_joining + "b, loading " + c.loading + ", target " + ab_target();
	if (status == level.s2x_ab_last_status)
		return;
	level.s2x_ab_last_status = status;
	ab_log(status);
}

ab_target()
{
	target = getdvarint("s2x_autobalance_target");
	slots = getdvarint("sv_maxclients");
	if (slots <= 0)
		slots = 18;
	if (target > slots)
		target = slots;
	if (target < 0)
		target = 0;
	return target;
}

ab_delay()
{
	delay = 15;
	if (getdvar("s2x_autobalance_delay") != "")
		delay = getdvarint("s2x_autobalance_delay");
	if (delay < 3)
		delay = 3;
	if (delay > 60)
		delay = 60;
	return delay;
}

ab_one_life()
{
	return maps\mp\_utility::getgametypenumlives() > 0;
}

// Humans and bots are told apart by the client, not by name. The field lets a private test
// stand a bot in for a human; nothing in a live match sets it.
ab_is_bot(player)
{
	if (isdefined(player.s2x_ab_test_human))
		return 0;
	return isbot(player) || istestclient(player);
}

// ---------------------------------------------------------------------------
// Counting
// ---------------------------------------------------------------------------

ab_count()
{
	c = spawnstruct();
	c.humans = [];
	c.humans["allies"] = [];
	c.humans["axis"] = [];
	c.bots = [];
	c.bots["allies"] = [];
	c.bots["axis"] = [];
	c.all_bots = [];
	c.bots_joining = 0;
	c.humans_joining = 0;
	c.real_humans = 0;

	foreach (player in level.players)
	{
		if (!isdefined(player))
			continue;

		team = player.pers["team"];
		if (ab_is_bot(player))
		{
			c.all_bots[c.all_bots.size] = player;
			if (isdefined(team) && (team == "allies" || team == "axis"))
				c.bots[team][c.bots[team].size] = player;
			else if (level.s2x_ab_teams)
				c.bots_joining++;
			continue;
		}

		if (!isbot(player) && !istestclient(player))
			c.real_humans++;

		if (isdefined(team) && (team == "allies" || team == "axis"))
		{
			// A player already promised to the other side counts there.
			if (isdefined(player.s2x_ab_pending_team))
				team = player.s2x_ab_pending_team;
			c.humans[team][c.humans[team].size] = player;
		}
		else if (!isdefined(team) || team != "spectator")
			c.humans_joining++;
	}

	// Humans still loading are connected but not yet in level.players.
	c.loading = ab_roster_humans() - c.real_humans;
	if (c.loading < 0)
		c.loading = 0;

	c.in_game = c.humans["allies"].size + c.humans["axis"].size + c.humans_joining;
	return c;
}

// getplayerroster() lists connected human clients as JSON; count its "id" keys.
ab_roster_humans()
{
	tokens = strtok(getplayerroster(), "\"");
	count = 0;
	for (i = 0; i + 1 < tokens.size; i++)
	{
		if (tokens[i] == "id" && tokens[i + 1] == ":")
			count++;
	}
	return count;
}

// ---------------------------------------------------------------------------
// Bots. One change per tick, recounted every time: the engine also removes bots on its own
// when a joining human takes the slot a bot was in.
// ---------------------------------------------------------------------------

ab_population(c)
{
	// Bots from a request still in flight are not counted yet; wait for them.
	if (level.s2x_ab_adding)
		return 0;

	target = ab_target();
	// Kick only for humans already in; add only for humans not still loading, or a bot added
	// now is kicked a moment later when that human arrives.
	keep = max(0, target - c.in_game);
	want = max(0, target - c.in_game - c.loading);

	if (!level.s2x_ab_teams)
	{
		if (c.all_bots.size > keep)
			return ab_kick(ab_pick_bot(c.all_bots));
		if (c.all_bots.size < want)
			return ab_add(want - c.all_bots.size);
		return 0;
	}

	if (c.bots_joining > 0)
		return 0;

	bots = c.bots["allies"].size + c.bots["axis"].size;
	desired = ab_bot_split(c, keep);
	surplus = [];
	surplus["allies"] = c.bots["allies"].size - desired["allies"];
	surplus["axis"] = c.bots["axis"].size - desired["axis"];

	if (bots > keep)
	{
		team = "allies";
		if (surplus["axis"] > surplus["allies"])
			team = "axis";
		// Never empty a side: that starts the forfeit timer.
		if (c.bots[team].size + c.humans[team].size <= 1)
			return 0;
		bot = ab_pick_bot(c.bots[team]);
		if (ab_one_life() && isalive(bot))
			return 0;
		return ab_kick(bot);
	}

	if (surplus["allies"] > 0 && surplus["axis"] < 0)
		return ab_move_bot(c.bots["allies"], "axis");
	if (surplus["axis"] > 0 && surplus["allies"] < 0)
		return ab_move_bot(c.bots["axis"], "allies");

	if (bots < want)
		return ab_add(want - bots);

	return 0;
}

// Split the bots so both sides end up the same size, humans first. Humans still choosing a
// team are assumed to take the smaller side, as auto-assign does.
ab_bot_split(c, bots)
{
	side = [];
	side["allies"] = c.humans["allies"].size;
	side["axis"] = c.humans["axis"].size;
	for (i = 0; i < c.humans_joining; i++)
	{
		if (side["allies"] <= side["axis"])
			side["allies"]++;
		else
			side["axis"]++;
	}

	split = [];
	split["allies"] = 0;
	split["axis"] = 0;
	for (i = 0; i < bots; i++)
	{
		if (side["allies"] + split["allies"] <= side["axis"] + split["axis"])
			split["allies"]++;
		else
			split["axis"]++;
	}
	return split;
}

// Prefer a dead bot: it leaves without anyone seeing it vanish mid-fight.
ab_pick_bot(list)
{
	for (i = list.size - 1; i >= 0; i--)
	{
		if (!isalive(list[i]))
			return list[i];
	}
	return list[list.size - 1];
}

ab_kick(bot)
{
	if (!isdefined(bot))
		return 0;
	ab_log("bot " + bot getentitynumber() + " leaves (" + ab_team_of(bot) + ")");
	kick(bot getentitynumber(), "EXE_PLAYERKICKED_BOT_BALANCE");
	wait 0.1;
	return 1;
}

ab_add(count)
{
	if (count > 4)
		count = 4;
	// A bot added mid-round in a one-life mode cannot spawn, and stock kicks it after a
	// minute; add only while the round is starting.
	if (ab_one_life() && gettime() - level.s2x_ab_round_start > 20000)
		return 0;

	level.s2x_ab_counter++;
	done = "s2x_ab_added_" + level.s2x_ab_counter;
	ab_log("adding " + count + " bot(s)");
	level.s2x_ab_adding = 1;
	level thread ab_spawn_bots(count, done);
	level thread ab_add_finished(done);
	return 1;
}

// spawn_bots notifies once every bot it added has spawned or given up, which can take up to
// a minute; no second request goes out meanwhile.
ab_add_finished(done)
{
	level thread ab_notify_after(done, 75);
	level waittill(done);
	level.s2x_ab_adding = 0;
}

ab_spawn_bots(count, done)
{
	level endon(done);
	// Stock auto-assign puts each bot on the side with fewer players; the next ticks fix
	// anything it gets wrong by moving bots.
	[[ level.bot_funcs["bots_spawn"] ]](count, "autoassign", undefined, 1, done);
}

ab_notify_after(message, seconds)
{
	level endon(message);
	wait seconds;
	level notify(message);
}

ab_move_bot(list, team)
{
	// Moving a dead bot is invisible; a live one has to go through the menu path, which kills it.
	bot = undefined;
	foreach (candidate in list)
	{
		if (!isalive(candidate))
		{
			bot = candidate;
			break;
		}
	}

	if (isdefined(bot))
	{
		ab_log("bot " + bot getentitynumber() + " switches to " + team);
		bot ab_move_dead(team);
		return 1;
	}

	if (ab_one_life())
		return 0;

	bot = list[list.size - 1];
	ab_log("bot " + bot getentitynumber() + " switches to " + team + " (menu)");
	bot._id_1AFA = team;
	bot notify("luinotifyserver", "team_select", ab_menu_team(team));
	waitframe();
	if (isdefined(bot) && isdefined(bot.bot_class))
		bot notify("luinotifyserver", "class_select", bot.bot_class);
	return 1;
}

ab_menu_team(team)
{
	if (team == "axis")
		return 0;
	return 1;
}

ab_team_of(player)
{
	if (isdefined(player.pers["team"]))
		return player.pers["team"];
	return "none";
}

ab_other(team)
{
	if (team == "allies")
		return "axis";
	return "allies";
}

ab_team_name(team)
{
	if (team == "axis")
		return "Axis";
	return "Allies";
}

// ---------------------------------------------------------------------------
// Humans
// ---------------------------------------------------------------------------

ab_humans(c)
{
	if (level.s2x_ab_counting_down)
		return;

	big = "allies";
	if (c.humans["axis"].size > c.humans["allies"].size)
		big = "axis";
	small = ab_other(big);
	gap = c.humans[big].size - c.humans[small].size;
	if (gap < 2)
	{
		level.s2x_ab_seen = 0;
		return;
	}

	// Ignore a single tick: someone mid-switch shows up on neither side for a moment.
	level.s2x_ab_seen++;
	if (level.s2x_ab_seen < 2)
		return;
	level.s2x_ab_seen = 0;

	if (ab_one_life())
	{
		ab_plan_next_round(c, big, small, int(gap / 2));
		return;
	}

	// An empty side starts the stock forfeit timer, so do not wait out the full countdown.
	delay = ab_delay();
	if (c.humans[small].size == 0 && c.bots[small].size == 0)
		delay = 3;
	level thread ab_countdown(delay);
}

ab_countdown(delay)
{
	level endon("game_ended");
	level.s2x_ab_counting_down = 1;

	iprintlnbold("^3Teams will auto-balance in " + delay + " seconds");
	ab_log("countdown " + delay + "s");
	if (isdefined(level.s2x_ab_text))
	{
		level.s2x_ab_timer settimer(delay);
		level.s2x_ab_text.alpha = 1;
		level.s2x_ab_timer.alpha = 1;
	}

	maps\mp\gametypes\_hostmigration::waitlongdurationwithhostmigrationpause(delay);

	if (isdefined(level.s2x_ab_text))
	{
		level.s2x_ab_text.alpha = 0;
		level.s2x_ab_timer.alpha = 0;
	}

	// Recount: people leave, join and switch during the countdown.
	c = ab_count();
	big = "allies";
	if (c.humans["axis"].size > c.humans["allies"].size)
		big = "axis";
	small = ab_other(big);
	gap = c.humans[big].size - c.humans[small].size;
	if (gap >= 2)
		ab_move_humans(c.humans[big], small, int(gap / 2));
	else
		ab_log("countdown ended already balanced");

	// Leave time for the moves to land before the next check.
	wait 3;
	level.s2x_ab_counting_down = 0;
}

ab_move_humans(list, team, count)
{
	// Random humans, and nobody who already has a move coming.
	candidates = [];
	foreach (player in list)
	{
		if (!isdefined(player.s2x_ab_pending_team))
			candidates[candidates.size] = player;
	}

	for (i = 0; i < count && candidates.size > 0; i++)
	{
		index = randomint(candidates.size);
		player = candidates[index];
		candidates[index] = candidates[candidates.size - 1];
		candidates[candidates.size - 1] = undefined;
		player thread ab_move_human(team);
	}
}

ab_move_human(team)
{
	self endon("disconnect");
	level endon("game_ended");

	if (!isalive(self) || self.sessionstate != "playing")
	{
		ab_log(self.name + " switches to " + team + " (not alive)");
		ab_move_dead(team);
		self iprintlnbold("^3Auto-balance moved you to " + ab_team_name(team));
		return;
	}

	if (getdvar("s2x_autobalance_live") != "0" && ab_can_swap_live())
	{
		ab_log(self.name + " switches to " + team + " (live)");
		ab_swap_live(team);
		self iprintlnbold("^3Auto-balance moved you to " + ab_team_name(team) + ", streak kept");
		return;
	}

	ab_log(self.name + " switches to " + team + " at next death");
	self.s2x_ab_pending_team = team;
	self iprintlnbold("^3Auto-balance: you join " + ab_team_name(team) + " when you respawn");
	self waittill("death");
	wait 0.05;
	self.s2x_ab_pending_team = undefined;
	if (!isalive(self))
		ab_move_dead(team);
}

// For a player who is dead, waiting to respawn or spectating on a team. The waitForChangeTeam
// notify ends the scorestreak listener that otherwise wipes every earned streak on
// joined_team; it re-arms at the next spawn. PlayerKilled already took the player out of the
// alive count, so addToTeam must not move the alive count again.
ab_move_dead(team)
{
	self notify("waitForChangeTeam");
	_id_04E8::_id_09FC(team);

	if (self.sessionstate == "spectator" && game["state"] == "playing")
	{
		_id_04E8::_id_36E3();
		self thread maps\mp\gametypes\_playerlogic::_id_9035();
	}

	ab_after_move(team);
}

ab_after_move(team)
{
	if (isbot(self) || istestclient(self))
		self._id_1AFA = team;
	else
		self setclientomnvar("ui_team_selected", ab_menu_team(team));
}

// A live swap is only safe for someone standing on the ground with nothing in hand: the
// streak system cannot follow a player riding a streak, carrying or placing one, holding a
// streak weapon or in last stand, and objective carriers are left alone.
ab_can_swap_live()
{
	if (ab_one_life() || !isalive(self) || self.sessionstate != "playing")
		return 0;
	gametype = getdvar("g_gametype");
	if (gametype == "ctf" || gametype == "ball")
		return 0;
	if (maps\mp\_utility::isusingremote() || maps\mp\_utility::_id_572D() || maps\mp\_utility::_id_56A8())
		return 0;
	if (isdefined(self._id_56A3) && self._id_56A3)
		return 0;
	if (isdefined(self.inlaststand) && self.inlaststand)
		return 0;
	if (self ismantling() || self ismeleeing() || !self isonground() || self isonladder() || self isusingturret())
		return 0;
	if (maps\mp\_utility::iskillstreakweapon(self getcurrentweapon()))
		return 0;
	return 1;
}

// The stock Infected switch: change team without dying, then a faux spawn, which re-runs the
// per-spawn scripts (the scorestreak one re-arms, re-issues team streak weapons and redraws
// the streak HUD from the kept progress), and the new side's loadout.
ab_swap_live(team)
{
	kept = ab_streak_snapshot();

	self notify("waitForChangeTeam");
	_id_04E8::_id_09FC(team, undefined, 1);

	spawn = self [[ level._id_4696 ]]();
	if (isdefined(spawn))
	{
		self setorigin(spawn.origin);
		self setplayerangles(spawn.angles);
	}

	self notify("faux_spawn");
	maps\mp\gametypes\_class::_id_4773(self.team, self.class);
	ab_after_move(team);

	waittillframeend;
	ab_streak_check(kept);
}

// Keep references to the streak structs, not copies: if anything wipes the slots, the structs
// themselves survive and go straight back.
ab_streak_snapshot()
{
	kept = spawnstruct();
	kept.slots = [];
	// getarraykeys() is not available to server scripts here; the key/value foreach is.
	if (isdefined(self.pers["killstreaks"]))
	{
		foreach (key, slot in self.pers["killstreaks"])
			kept.slots[key] = slot;
	}
	kept.points = self.pers["ks_totalPoints"];
	kept.support = self.pers["ks_totalPointsSupport"];
	return kept;
}

ab_streak_check(kept)
{
	lost = 0;
	foreach (key, slot in kept.slots)
	{
		if (!isdefined(self.pers["killstreaks"][key]) || self.pers["killstreaks"][key] != slot)
			lost = 1;
	}
	if (isdefined(kept.points) && self.pers["ks_totalPoints"] != kept.points)
		lost = 1;
	if (!lost)
		return;

	ab_log(self.name + " streak restored after the switch");
	foreach (key, slot in kept.slots)
		self.pers["killstreaks"][key] = slot;
	if (isdefined(kept.points))
	{
		self._id_0A06 = kept.points;
		self.pers["ks_totalPoints"] = kept.points;
	}
	if (isdefined(kept.support))
	{
		self._id_0A0D = kept.support;
		self.pers["ks_totalPointsSupport"] = kept.support;
	}
	self notify("faux_spawn");
}

// ---------------------------------------------------------------------------
// One-life modes: humans switch between rounds. Players are put back on pers["team"] after
// the map restart, so rewriting it before the restart moves them without a death.
// ---------------------------------------------------------------------------

ab_plan_next_round(c, big, small, count)
{
	if (isdefined(level.s2x_ab_round_plan))
		return;
	level.s2x_ab_round_plan = [];

	candidates = [];
	foreach (player in c.humans[big])
		candidates[candidates.size] = player;
	for (i = 0; i < count && candidates.size > 0; i++)
	{
		index = randomint(candidates.size);
		level.s2x_ab_round_plan[level.s2x_ab_round_plan.size] = candidates[index];
		candidates[index] = candidates[candidates.size - 1];
		candidates[candidates.size - 1] = undefined;
	}

	iprintlnbold("^3Teams will auto-balance at the start of the next round");
	ab_log("next round: " + level.s2x_ab_round_plan.size + " switch(es) to " + small);
	foreach (player in level.s2x_ab_round_plan)
		player.s2x_ab_round_team = small;
}

// No game_ended endon: the round boundary comes after it.
ab_round_boundary()
{
	level waittill("restarting");
	if (!isdefined(level.s2x_ab_round_plan))
		return;
	foreach (player in level.s2x_ab_round_plan)
	{
		if (!isdefined(player) || !isdefined(player.s2x_ab_round_team))
			continue;
		team = player.s2x_ab_round_team;
		from = ab_other(team);
		player notify("waitForChangeTeam");
		player.pers["team"] = team;
		ab_log(player.name + " starts next round on " + team);

		// A bot goes the other way, so the side totals stay even.
		foreach (bot in level.players)
		{
			if (isdefined(bot) && ab_is_bot(bot) && !isdefined(bot.s2x_ab_round_swapped) && isdefined(bot.pers["team"]) && bot.pers["team"] == team)
			{
				bot.s2x_ab_round_swapped = 1;
				bot.pers["team"] = from;
				bot._id_1AFA = from;
				break;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Countdown display: one fixed label and a numeric timer that the client counts down itself.
// Built once per round after prematch, when the HUD parent exists.
// ---------------------------------------------------------------------------

ab_prematch_then_hud()
{
	level endon("game_ended");
	level thread ab_prematch_timeout();
	level waittill("s2x_ab_prematch");
	level.s2x_ab_prematch_done = 1;

	if (!level.s2x_ab_teams)
		return;

	text = maps\mp\gametypes\_hud_util::_id_2829("default", 1.4);
	text maps\mp\gametypes\_hud_util::setpoint("CENTER", undefined, 0, -110);
	text settext("Teams will auto-balance in");
	text.alpha = 0;

	timer = maps\mp\gametypes\_hud_util::_id_282B("default", 1.4);
	timer maps\mp\gametypes\_hud_util::setpoint("CENTER", undefined, 0, -88);
	timer.alpha = 0;

	level.s2x_ab_text = text;
	level.s2x_ab_timer = timer;
}

ab_prematch_timeout()
{
	level endon("s2x_ab_prematch");
	level thread ab_prematch_relay();
	wait 45;
	level notify("s2x_ab_prematch");
}

ab_prematch_relay()
{
	level endon("s2x_ab_prematch");
	level waittill("prematch_over");
	level notify("s2x_ab_prematch");
}
