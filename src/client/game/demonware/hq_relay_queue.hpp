#pragma once

#include "hq_event_relay.hpp"
#include <deque>
#include <mutex>

namespace demonware::hq_event_relay
{
	class client_queue
	{
	public:
		static constexpr std::size_t capacity = 256, batch_size = 32;

		bool push(const reward_game_events::event& event)
		{
			std::lock_guard lock{mutex_};
			// Drop newest: overload must not evict or reorder accepted work.
			if (pending_.size() == capacity) return false;
			pending_.push_back(event);
			return true;
		}

		std::vector<reward_game_events::event> take()
		{
			std::vector<reward_game_events::event> batch;
			batch.reserve(batch_size);
			std::lock_guard lock{mutex_};
			while (!pending_.empty() && batch.size() < batch_size)
			{
				batch.push_back(std::move(pending_.front()));
				pending_.pop_front();
			}
			return batch;
		}

	private:
		std::mutex mutex_;
		std::deque<reward_game_events::event> pending_;
	};

	// Capacity counts whole events, including an event partly sent this frame.
	// At most 16 MiB of encoded payload is retained (2048 * maximum_wire).
	class server_queue
	{
	public:
		static constexpr std::size_t capacity = 2048;

		reward_delivery push(const std::uint64_t user, const reward_game_events::event& event)
		{
			auto wire = encode(user, event);
			if (wire.empty()) return reward_delivery::retryable_failure;
			std::lock_guard lock{mutex_};
			if (pending_.size() == capacity) return reward_delivery::retryable_failure;
			pending_.push_back({user, std::move(wire), 0});
			return reward_delivery::queued;
		}

		std::vector<std::pair<std::uint64_t, std::string>> take(const std::size_t limit)
		{
			std::vector<std::pair<std::uint64_t, std::string>> result;
			result.reserve(limit);
			std::lock_guard lock{mutex_};
			while (!pending_.empty() && result.size() < limit)
			{
				auto& event = pending_.front();
				auto parts = chunks(event.user, event.wire);
				while (event.next < parts.size() && result.size() < limit)
				{
					result.emplace_back(event.user, std::move(parts[event.next]));
					++event.next;
				}
				if (event.next == parts.size()) pending_.pop_front();
			}
			return result;
		}

		void clear()
		{
			std::lock_guard lock{mutex_};
			pending_.clear();
		}

	private:
		struct pending_event
		{
			std::uint64_t user;
			std::string wire;
			std::size_t next;
		};
		std::mutex mutex_;
		std::deque<pending_event> pending_;
	};
}
