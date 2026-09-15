#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace demonware
{
	class hq_contract_clock
	{
	public:
		using clock = std::chrono::steady_clock;

		std::uint32_t sample(const bool playing, const clock::time_point now)
		{
			if (playing && last_) pending_ += now - *last_;
			last_ = playing ? std::optional{now} : std::nullopt;
			const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(pending_).count();
			return seconds > UINT32_MAX ? UINT32_MAX : static_cast<std::uint32_t>(seconds);
		}

		// A failed transaction never acknowledges its interval; keep fractions too.
		void committed(const std::uint32_t seconds) { pending_ -= std::chrono::seconds{seconds}; }

	private:
		std::optional<clock::time_point> last_;
		clock::duration pending_{};
	};
}
