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
