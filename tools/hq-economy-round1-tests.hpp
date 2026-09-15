#pragma once

// Included by the standalone HQ harness after its request()/require() helpers.
inline void round1_receipt_tests()
{
	const auto read = []
	{
		std::string bytes;
		require(utils::io::read_file("players2/user/hq_economy.json", &bytes), "round1 read store");
		return bytes;
	};
	const auto original = read();
	const auto rejected = [&](const std::string& tx, const std::string& json_tx)
	{
		const auto before = read();
		for (const auto* action : {"claim_achievement_reward", "open_supply_drop"})
		{
			const auto reply = request(std::string{"{\"Action\":\""} + action +
				"\",\"AchievementName\":\"daily_ch_kills\",\"SupplyDropID\":\"sd_mp\",\"ClientTx\":\"" + json_tx + "\"}");
			require(std::string_view{reply["Status"].GetString()} == "error", "reject AE receipt identifier");
			require(read() == before, "rejected AE identifier preserves bytes before reconciliation");
		}
		require(hq_marketplace::purchase(tx, hq_marketplace::vendor_skus[0].id, 1) ==
			BD_MARKETPLACE_INVALID_PARAMETER, "reject purchase identifier");
		require(!hq_marketplace::pawn(tx, {}), "reject pawn identifier");
		require(read() == before, "rejected marketplace identifier preserves bytes");
	};
	rejected(std::string(256, 'x'), std::string(256, 'x'));
	rejected(std::string(124, 'x'), std::string(124, 'x'));
	rejected(std::string{"x\0y", 3}, "x\\u0000y");
	const std::string limit(hq_economy::identifier_limit, 'k');
	require(hq_economy::transact([&](auto& state)
	{
		state.transactions[limit] = std::string{"x\0y", 3};
		return true;
	}), "exact receipt limit accepted");
	hq_economy::invalidate();
	require(hq_economy::snapshot().transactions.at(limit) == std::string("x\0y", 3),
		"exact receipt limit and explicit-length payload round trip");
	const auto good = read();
	for (const auto& key : {std::string{}, std::string(129, 'x'), std::string{"x\0y", 3}})
	{
		require(!hq_economy::transact([&](auto& state) { state.transactions[key] = "test"; return true; }),
			"save rejects invalid receipt");
		require(read() == good, "invalid save preserves original");
	}
	for (const auto& mutation : std::vector<std::function<bool(hq_economy::state&)>>{
		[](auto& s) { s.transactions["large-payload"] = std::string(1025, 'x'); return true; },
		[](auto& s) { s.achievements.begin()->second.challenge_name = std::string{"x\0y", 3}; return true; },
		[](auto& s) { s.achievements.begin()->second.rewards.resize(101); return true; },
		[](auto& s) { for (unsigned i = 0; i <= 10000; ++i) s.transactions["cap:" + std::to_string(i)] = ""; return true; },
		[](auto& s) { s.transactions.clear(); for (unsigned i = 0; i < 9990; ++i) s.transactions["size:" + std::to_string(i)] = std::string(1024, '\1'); return true; }})
	{
		require(!hq_economy::transact(mutation), "save enforces loader field/count/file limits");
		require(read() == good, "invalid state preserves bytes");
	}
	auto invalid = good;
	const auto pos = invalid.find(limit);
	require(pos != std::string::npos, "find receipt in JSON");
	invalid.replace(pos, limit.size(), "x\\u0000y");
	require(utils::io::write_file("players2/user/hq_economy.json", invalid, false), "write malformed fixture");
	hq_economy::invalidate();
	bool threw{};
	try { hq_economy::snapshot(); } catch (const std::exception&) { threw = true; }
	require(threw && read() == invalid, "NUL identifier load rejected without touching file");
	require(utils::io::write_file("players2/user/hq_economy.json", original, false), "restore fixture");
	hq_economy::invalidate();
	std::cout << "PASS: round1 receipt identifiers, explicit lengths, loader/save limits, byte preservation\n";
}

inline void round1_replay_tests()
{
	// Seed the old format at the window boundary, with keys after every decimal hash.
	// The old hash-order eviction immediately discarded each new receipt in this fixture.
	require(hq_economy::transact([](auto& state)
	{
		state.transactions.clear();
		for (unsigned i = 0; i < 2048; ++i) state.transactions["event:z" + std::to_string(i)] = "123";
		hq_economy::achievement entry;
		entry.name = "round1_kills"; entry.status = "inProgress"; entry.target = 10000;
		state.achievements[entry.name] = entry;
		return true;
	}), "seed full legacy replay window");
	achievement_engine::set_event_rules({{"round1_kills", {1, ""}}});
	for (unsigned i = 0; i < 2052; ++i)
	{
		const reward_game_events::event event{"killed_a_player", 9000000 + i, {}};
		require(achievement_engine::submit_event(event, true), "insert event at/beyond replay window");
		hq_economy::invalidate();
		const auto before = hq_economy::snapshot();
		const auto mtime = std::filesystem::last_write_time("players2/user/hq_economy.json");
		require(before.achievements.at("round1_kills").progress == i + 1, "one increment per new event");
		require(achievement_engine::submit_event(event, true), "retransmit newest event after reload");
		const auto after = hq_economy::snapshot();
		require(after.revision == before.revision &&
			std::filesystem::last_write_time("players2/user/hq_economy.json") == mtime, "duplicate event preserves revision and mtime");
		require(after.achievements.at("round1_kills").progress == i + 1, "retransmission never increments twice");
		require(std::count_if(after.transactions.begin(), after.transactions.end(),
			[](const auto& pair) { return pair.first.starts_with("event:"); }) == 2048, "replay window stays bounded");
	}
	std::cout << "PASS: round1 replay insertion order across reload, 2052 events and retransmissions, duplicate revision/mtime unchanged\n";
}
