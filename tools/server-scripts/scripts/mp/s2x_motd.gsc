// s2x_motd.gsc - welcome message, an occasional chat reminder and timed trivia.
//
// Server-side only. Copy this file to <game>\s2x\scripts\mp\ on the server box;
// S2x compiles every .gsc under scripts\mp\ when a map loads and runs init().
// Players need nothing extra. The console shows
//   Executing 'scripts/mp/s2x_motd::init'
// on load, or a "script compile error" block if the file does not parse.
//
// Everything a server owner would tune is in init(). Text supports S2 colour
// codes: ^1 red, ^2 green, ^3 yellow, ^5 cyan, ^7 white.

init()
{
	// Shown to each player shortly after their first spawn, one line at a time.
	level.motd_lines = [];
	level.motd_lines[0] = "^3Welcome to the server";
	level.motd_lines[1] = "^7Relaxed score limits, bots fill empty slots";
	level.motd_lines[2] = "^7Play fair and have fun";
	level.motd_delay = 3;          // seconds after spawn before the first line
	level.motd_line_gap = 3;       // seconds between lines

	// Small lower-left reminder, shown to everyone. Infrequent on purpose.
	level.chat_reminder = "^7Press ^3T ^7to chat with the lobby";
	level.chat_reminder_interval = 600;

	// Trivia: question, pause, answer. Questions come from trivia_questions().
	level.trivia_first_delay = 90;  // seconds into the match before the first question
	level.trivia_answer_delay = 20; // seconds the question stays open
	level.trivia_interval = 240;    // seconds from one answer to the next question

	level.trivia = [];
	trivia_questions();

	level thread on_player_connect();
	level thread chat_reminder_loop();
	level thread trivia_loop();
}

// ---------------------------------------------------------------------------
// Message of the day
// ---------------------------------------------------------------------------

on_player_connect()
{
	for (;;)
	{
		level waittill("connected", player);
		player thread show_motd_on_first_spawn();
	}
}

show_motd_on_first_spawn()
{
	self endon("disconnect");
	level endon("game_ended");

	if (isbot(self) || istestclient(self))
	{
		return;
	}

	self waittill("spawned_player");
	wait level.motd_delay;

	for (i = 0; i < level.motd_lines.size; i++)
	{
		self iPrintLnBold(level.motd_lines[i]);
		if (i + 1 < level.motd_lines.size)
		{
			wait level.motd_line_gap;
		}
	}
}

// ---------------------------------------------------------------------------
// Chat reminder
// ---------------------------------------------------------------------------

chat_reminder_loop()
{
	level endon("game_ended");

	for (;;)
	{
		wait level.chat_reminder_interval;

		if (!isdefined(level.players))
		{
			continue;
		}

		for (i = 0; i < level.players.size; i++)
		{
			player = level.players[i];
			if (!isdefined(player) || isbot(player) || istestclient(player))
			{
				continue;
			}

			player iPrintLn(level.chat_reminder);
		}
	}
}

// ---------------------------------------------------------------------------
// Trivia
// ---------------------------------------------------------------------------

trivia_loop()
{
	level endon("game_ended");

	if (level.trivia.size == 0)
	{
		return;
	}

	order = shuffled_indices(level.trivia.size);
	next = 0;

	wait level.trivia_first_delay;

	for (;;)
	{
		if (next >= order.size)
		{
			order = shuffled_indices(level.trivia.size);
			next = 0;
		}

		entry = level.trivia[order[next]];
		next++;

		if (humans_present())
		{
			iPrintLnBold("^5Trivia: ^7" + entry.question);
			wait level.trivia_answer_delay;
			iPrintLnBold("^5Answer: ^7" + entry.answer);
		}

		wait level.trivia_interval;
	}
}

add_question(question, answer)
{
	entry = spawnstruct();
	entry.question = question;
	entry.answer = answer;
	level.trivia[level.trivia.size] = entry;
}

trivia_questions()
{
	add_question("On what date did the Allies land in Normandy?", "June 6, 1944");
	add_question("What was the codename for the Normandy invasion?", "Operation Overlord");
	add_question("Which forest was the setting for the Battle of the Bulge?", "The Ardennes");
	add_question("Which US general replied 'Nuts!' to a surrender demand at Bastogne?", "Anthony McAuliffe");
	add_question("Which US division is nicknamed the Big Red One?", "The 1st Infantry Division");
	add_question("Which airborne division is known as the Screaming Eagles?", "The 101st Airborne");
	add_question("What was the first German city captured by the Allies?", "Aachen, in October 1944");
	add_question("Which bridge did US troops seize at Remagen in March 1945?", "The Ludendorff Bridge");
	add_question("What was the standard US infantry rifle of the war?", "The M1 Garand");
	add_question("Which German rifle is considered the first true assault rifle?", "The StG 44");
	add_question("What does BAR stand for?", "Browning Automatic Rifle");
	add_question("Which US submachine gun was nicknamed the Grease Gun?", "The M3");
	add_question("Which landing craft carried troops onto Omaha Beach?", "The Higgins boat, the LCVP");
	add_question("On what date did the war in Europe end?", "May 8, 1945, VE Day");
	add_question("Which British site broke the Enigma cipher?", "Bletchley Park");
	add_question("Which battle ended in a German surrender in February 1943?", "Stalingrad");
	add_question("Operation Market Garden took place in which country?", "The Netherlands");
	add_question("Where did Roosevelt, Churchill and Stalin meet in February 1945?", "Yalta");
	add_question("Which US tank was the workhorse of the Western Front?", "The M4 Sherman");
	add_question("Which German heavy tank carried the feared 88mm gun?", "The Tiger");
	add_question("Which aircraft dropped the atomic bomb on Hiroshima?", "The Enola Gay, a B-29");
	add_question("What was the codename of the US atomic bomb programme?", "The Manhattan Project");
	add_question("Which operation opened the German invasion of the Soviet Union?", "Operation Barbarossa, June 1941");
	add_question("Who was Supreme Commander of the Allied forces in Europe?", "Dwight D. Eisenhower");
	add_question("Which US general led the Third Army across France?", "George S. Patton");
	add_question("Which US pilots were nicknamed the Red Tails?", "The Tuskegee Airmen");
	add_question("What was the Allied breakout from Normandy in July 1944 called?", "Operation Cobra");
	add_question("What was Germany's line of coastal fortifications called?", "The Atlantic Wall");
	add_question("Which battle was the US Army's longest fight on German soil?", "The Hurtgen Forest");
	add_question("Which studio developed Call of Duty: WWII?", "Sledgehammer Games");
	add_question("In which year did Call of Duty: WWII release?", "2017");
	add_question("Which game first featured the map Shipment?", "Call of Duty 4: Modern Warfare");
	add_question("What is the first Nazi Zombies map in Call of Duty: WWII?", "The Final Reich");
	add_question("Who is the playable protagonist of the WWII campaign?", "Private Ronald 'Red' Daniels");
	add_question("Which War Mode map recreates the Omaha Beach landing?", "Operation Neptune");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

humans_present()
{
	if (!isdefined(level.players))
	{
		return false;
	}

	for (i = 0; i < level.players.size; i++)
	{
		player = level.players[i];
		if (!isdefined(player))
		{
			continue;
		}

		if (isbot(player) || istestclient(player))
		{
			continue;
		}

		return true;
	}

	return false;
}

shuffled_indices(count)
{
	order = [];
	for (i = 0; i < count; i++)
	{
		order[i] = i;
	}

	for (i = count - 1; i > 0; i--)
	{
		j = randomint(i + 1);
		swap = order[i];
		order[i] = order[j];
		order[j] = swap;
	}

	return order;
}
