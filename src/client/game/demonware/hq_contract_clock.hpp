#pragma once

#include "achievement_engine.hpp"
#include <chrono>
#include <cstdint>
#include <optional>

namespace demonware
{
	class hq_contract_clock
	{
	public:
		using clock = std::chrono::steady_clock;

		void tick(const bool playing, const clock::time_point now)
		{
			// Observe play boundaries even when the store cannot be read.
			for (auto& [name, timer] : timers_)
			{
				if (playing && timer.last) timer.pending += now - *timer.last;
				timer.last = playing ? std::optional{now} : std::nullopt;
			}
			const auto data = hq_economy::snapshot(); // Cached read; idle timers never open a transaction.
			std::erase_if(timers_, [&](const auto& pair)
			{
				const auto it = data.achievements.find(pair.first);
				return it == data.achievements.end() || !pair.second.matches(it->second);
			});
			bool due{};
			for (const auto& [name, entry] : data.achievements)
			{
				if (entry.kind != 4 || entry.status != "inProgress" || !entry.usage_target) continue;
				auto [it, inserted] = timers_.try_emplace(name);
				auto& timer = it->second;
				if (inserted)
				{
					timer.activation = entry.activation;
					timer.generation = entry.activation_generation;
					// New activations start here; existing timers were already sampled above.
					timer.last = playing ? std::optional{now} : std::nullopt;
				}
				due |= timer.seconds() != 0;
			}
			if (!due) return;
			if (hq_economy::transact([&](auto& next)
			{
				for (const auto& [name, timer] : timers_)
				{
					const auto it = next.achievements.find(name);
					// The activation may have changed between the snapshot and the disk lock.
					if (it != next.achievements.end() && timer.matches(it->second))
						achievement_engine::advance_contract_time(it->second, timer.seconds());
				}
				return true;
			}))
				for (auto& [name, timer] : timers_) timer.pending -= std::chrono::seconds{timer.seconds()};
			// Failed debits and fractional seconds stay with their original activation.
		}

	private:
		struct timer
		{
			std::uint64_t activation{}, generation{};
			std::optional<clock::time_point> last;
			clock::duration pending{};

			bool matches(const hq_economy::achievement& entry) const
			{
				return entry.kind == 4 && entry.status == "inProgress" && entry.usage_target &&
					entry.activation == activation && entry.activation_generation == generation;
			}

			std::uint32_t seconds() const
			{
				const auto value = std::chrono::duration_cast<std::chrono::seconds>(pending).count();
				return value > UINT32_MAX ? UINT32_MAX : static_cast<std::uint32_t>(value);
			}
		};
		std::map<std::string, timer> timers_;
	};
}
