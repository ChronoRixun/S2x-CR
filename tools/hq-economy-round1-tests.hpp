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
	const auto boundary_bytes = read();
	const auto claim = request("{\"Action\":\"claim_achievement_reward\",\"AchievementName\":\"daily_ch_kills\",\"ClientTx\":\"" + std::string(123, 'x') + "\"}");
	require(std::string_view{claim["reason"].GetString()} == "invalid_achievement", "claim prefix consumes six bytes");
	require(hq_marketplace::purchase(std::string(120, 'x'), 2, 1) == BD_MARKETPLACE_INVALID_PARAMETER,
		"purchase prefix consumes nine bytes");
	byte_buffer pawn_request;
	pawn_request.write_string("s2_steam"); pawn_request.write_string(std::string(124, 'x')); pawn_request.write_uint32(0);
	byte_buffer pawn_wire{pawn_request.get_buffer()};
	std::string pawn_tx;
	std::vector<hq_economy::item> pawn_items;
	require(!hq_marketplace::parse_pawn(&pawn_wire, pawn_tx, pawn_items), "pawn parser rejects overlong key");
	require(read() == boundary_bytes, "boundary rejection leaves bytes unchanged");
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
		[](auto& s) { auto node = s.achievements.extract(s.achievements.begin()); node.key() += "invalid"; s.achievements.insert(std::move(node)); return true; },
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
	achievement_engine::set_catalog({});
	achievement_engine::set_loot_catalog({0x20000D});
	require(hq_economy::transact([](auto& state)
	{
		hq_economy::achievement entry;
		entry.name = "round1_claim"; entry.kind = 5; entry.status = "claimable"; entry.progress = 1;
		entry.rewards = {{"GRANT_CURRENCY", hq_economy::armory_credits, 1}};
		state.achievements[entry.name] = entry;
		state.currencies[hq_economy::armory_credits] = 100000;
		return hq_economy::grant(state, {"GRANT_PRODUCT", 1, 1});
	}), "seed eligible claim, drop and purchase");
	const auto accepted_claim = request("{\"Action\":\"claim_achievement_reward\",\"AchievementName\":\"round1_claim\",\"ClientTx\":\"" + std::string(122, 'c') + "\"}");
	const auto accepted_drop = request("{\"Action\":\"open_supply_drop\",\"SupplyDropID\":\"sd_mp\",\"ClientTx\":\"" + std::string(123, 'd') + "\"}");
	require(std::string_view{accepted_claim["Status"].GetString()} == "ok" &&
		std::string_view{accepted_drop["Status"].GetString()} == "ok", "claim/drop accept exact prefix budgets");
	require(hq_marketplace::purchase(std::string(119, 'p'), 2, 1) == BD_NO_ERROR &&
		hq_marketplace::pawn(std::string(123, 'w'), {}), "purchase/pawn accept exact prefix budgets");
	hq_economy::invalidate();
	const auto roundtrip = hq_economy::snapshot();
	for (const auto& key : {"claim:" + std::string(122, 'c'), "drop:" + std::string(123, 'd'),
		"purchase:" + std::string(119, 'p'), "pawn:" + std::string(123, 'w')})
		require(roundtrip.transactions.contains(key), "every exact-limit producer receipt survives reload");
	require(utils::io::write_file("players2/user/hq_economy.json", original, false), "restore producer fixture");
	hq_economy::invalidate();
	std::cout << "PASS: round1 receipt identifiers, explicit lengths, loader/save limits, byte preservation\n";
}

inline void round1_log_and_capacity_tests()
{
	const auto reply = request(R"({"Action":"round1\n\t\u0000'\\","UserIDs":["PRIVATE_USER_SENTINEL"],"ClientTx":"PRIVATE_TX_SENTINEL"})");
	require(std::string_view{reply["reason"].GetString()} == "unsupported_action", "unsupported action rejected");
	require(console::warnings.back() == "[HQ AE] unsupported action 'round1?????': unsupported_action\n", "escaped action only in normal warning");
	request("{\"Action\":\"" + std::string(100, 'a') + "\",\"UserIDs\":[\"PRIVATE_USER_SENTINEL\"]}");
	require(console::warnings.back() == "[HQ AE] unsupported action '" + std::string(64, 'a') + "': unsupported_action\n", "action log bounded to 64 characters");
	for (const auto& line : console::warnings)
		require(line.find("PRIVATE_") == std::string::npos, "normal warnings omit identifiers and body");
	std::string original;
	require(utils::io::read_file("players2/user/hq_economy.json", &original), "save capacity fixture original");
	const auto cached_revision = hq_economy::snapshot().revision;
	rapidjson::Document external;
	external.Parse(original.data(), original.size());
	external["revision"].SetUint64(cached_revision + 1);
	rapidjson::StringBuffer encoded;
	rapidjson::Writer<rapidjson::StringBuffer> writer{encoded};
	external.Accept(writer);
	require(utils::io::write_file("players2/user/hq_economy.json", {encoded.GetString(), encoded.GetSize()}, false), "simulate another instance commit");
	require(hq_economy::snapshot().revision == cached_revision, "cached reads retain old external revision");
	require(!hq_economy::transact([](auto&) { return false; }) && hq_economy::snapshot().revision == cached_revision,
		"rejected mutation does not refresh cached reads");
	const auto external_mtime = std::filesystem::last_write_time("players2/user/hq_economy.json");
	require(hq_economy::transact([](auto&) { return true; }) && hq_economy::snapshot().revision == cached_revision + 1 &&
		std::filesystem::last_write_time("players2/user/hq_economy.json") == external_mtime, "successful no-op observes external revision without writing");
	require(hq_economy::transact([](auto& state)
	{
		for (unsigned i = 0; state.transactions.size() < 10000; ++i) state.transactions["capacity:" + std::to_string(i)] = "";
		return true;
	}), "10000 receipts remain loadable");
	hq_economy::invalidate();
	const auto full = hq_economy::snapshot();
	const auto mtime = std::filesystem::last_write_time("players2/user/hq_economy.json");
	for (unsigned i = 0; i < 2; ++i)
		require(!hq_economy::transact([](auto& state) { state.transactions["capacity:blocked"] = ""; return true; }), "full ledger blocks new receipt");
	require(hq_economy::snapshot().revision == full.revision &&
		std::filesystem::last_write_time("players2/user/hq_economy.json") == mtime, "ceiling failure preserves revision and mtime");
	const std::string warning = "[HQ economy] Receipt limit (10000) reached in players2/user/hq_economy.json; new receipts cannot be saved. Deleting players2/user/hq_economy.json resets only the Headquarters economy.\n";
	require(std::count(console::warnings.begin(), console::warnings.end(), warning) == 1, "one capacity warning per session");
	require(utils::io::write_file("players2/user/hq_economy.json", original, false), "restore capacity fixture");
	hq_economy::invalidate();
	std::cout << "PASS: round1 bounded escaped logs, no request identifiers, cached external reads, full ledger warning once\n";
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
