#pragma once

#include "hq_event_relay.hpp"
#include <deque>
#include <mutex>
#include <list>
#include <functional>
#include <set>

namespace demonware::hq_event_relay
{
	class client_queue
	{
	public:
		static constexpr std::size_t capacity = 256, batch_size = 32;

		bool full()
		{
			std::lock_guard lock{mutex_};
			return pending_.size() == capacity;
		}

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
	// Bound both event slots and encoded bytes, including outstanding reservations.
	class server_queue
	{
	public:
		// Task 11 accepts 48 * 100 events; leave 320 slots for smaller queued requests.
		static constexpr std::size_t capacity = 5120;
		// Decimal fields expand by at most 3x, plus under 64 header bytes/event:
		// 3 * 3 MiB + 4800 * 64 < 10 MiB. Keep 2 MiB additional headroom.
		static constexpr std::size_t byte_capacity = 12 * 1024 * 1024;

		reward_delivery push(const std::uint64_t user, const reward_game_events::event& event)
		{
			return push_batch({{user, event}}, [] { return true; });
		}

		reward_delivery push_batch(const std::vector<std::pair<std::uint64_t, reward_game_events::event>>& events,
			const std::function<bool()>& apply_local)
		{
			// Allocate and encode before accepting anything; publication is a no-throw splice.
			std::list<pending_event> prepared;
			std::size_t bytes{};
			for (const auto& [user, event] : events)
			{
				auto wire = encode(user, event);
				if (wire.empty()) return reward_delivery::permanent_failure;
				if (prepared.size() == capacity || wire.size() > byte_capacity - bytes)
					return reward_delivery::retryable_failure;
				bytes += wire.size();
				prepared.push_back({user, std::move(wire), 0});
			}
			const auto count = prepared.size();
			{
				std::lock_guard lock{mutex_};
				if (count > capacity - pending_.size() - reserved_ || bytes > byte_capacity - pending_bytes_ - reserved_bytes_) return reward_delivery::retryable_failure;
				reserved_ += count;
				reserved_bytes_ += bytes;
			}
			// Request-scoped ownership releases capacity on failure or exception.
			struct reservation
			{
				server_queue& queue;
				std::size_t count, bytes;
				~reservation()
				{
					std::lock_guard lock{queue.mutex_};
					queue.reserved_ -= count;
					queue.reserved_bytes_ -= bytes;
				}
			} reserved{*this, count, bytes};
			if (!apply_local()) return reward_delivery::retryable_failure;
			{
				std::lock_guard lock{mutex_};
				pending_.splice(pending_.end(), prepared);
				reserved_ -= count;
				reserved_bytes_ -= bytes;
				pending_bytes_ += bytes;
				reserved.count = reserved.bytes = 0;
			}
			return count ? reward_delivery::queued : reward_delivery::applied;
		}

		std::vector<std::pair<std::uint64_t, std::string>> take(const std::size_t limit,
			const std::function<bool(std::uint64_t)>& can_take = [](std::uint64_t) { return true; })
		{
			std::vector<std::pair<std::uint64_t, std::string>> result;
			result.reserve(limit);
			std::lock_guard lock{mutex_};
			std::set<std::uint64_t> blocked;
			for (auto it = pending_.begin(); it != pending_.end() && result.size() < limit;)
			{
				auto& event = *it;
				// Keep each user's whole-event order while other users pass a blocked one.
				if (blocked.contains(event.user)) { ++it; continue; }
				// Resume a paused event at its next fragment; block all later events for this user.
				// Within one uninterrupted connection, the possibly lagging receiver sees ordered
				// continuations or the next event's fragment zero, so pauses never orphan a partial.
				auto parts = chunks(event.user, event.wire);
				while (event.next < parts.size() && result.size() < limit)
				{
					if (!can_take(event.user)) { blocked.insert(event.user); break; }
					result.emplace_back(event.user, std::move(parts[event.next]));
					++event.next;
				}
				if (event.next == parts.size())
				{
					pending_bytes_ -= event.wire.size();
					it = pending_.erase(it);
				}
				else ++it;
			}
			return result;
		}

		void clear()
		{
			std::lock_guard lock{mutex_};
			pending_.clear();
			pending_bytes_ = 0;
		}

	private:
		struct pending_event
		{
			std::uint64_t user;
			std::string wire;
			std::size_t next;
		};
		std::mutex mutex_;
		std::list<pending_event> pending_;
		std::size_t reserved_{}, pending_bytes_{}, reserved_bytes_{};
	};
}
