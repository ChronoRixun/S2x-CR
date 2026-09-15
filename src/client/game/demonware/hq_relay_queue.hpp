#pragma once

#include "reward_game_event.hpp"
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
}
