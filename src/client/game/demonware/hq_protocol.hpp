#pragma once

#include "byte_buffer.hpp"
#include "data_types.hpp"
#include "component/console/console.hpp"
#include <utils/flags.hpp>
#include <utils/io.hpp>

namespace demonware::hq_protocol
{
	// A structured reply still needs its typed length-delimited body when empty.
	class empty_struct_result final : public bdTaskResult
	{
	public:
		void serialize(byte_buffer* buffer) override
		{
			char empty{};
			buffer->write_struct(&empty, 0);
		}
	};

	inline bool padding(byte_buffer* buffer)
	{
		const auto tail = buffer->get_remaining();
		// pump_global_achievement_counters arrives with 17 trailing zero bytes; accept any short zero tail.
		return tail.size() <= 64 && std::all_of(tail.begin(), tail.end(), [](char value) { return value == 0; });
	}

	inline bool parse_ae(byte_buffer* buffer, std::string& json)
	{
		std::string context{};
		unsigned short count{};
		int type{};
		return buffer->size() <= 65536 && buffer->read_string(&context) && context == "s2_steam" &&
			buffer->read_uint16(&count) && count == 1 && buffer->read_int32(&type) && type == 1 &&
			buffer->read_string(&json) && padding(buffer);
	}

	inline void write_ae(byte_buffer* buffer, const std::string& json)
	{
		buffer->write_string("s2_steam");
		buffer->write_uint16(1);
		buffer->write_int32(1);
		buffer->write_string(json);
	}

	inline void trace(const char* label, const std::string& bytes)
	{
		// Raw dumps only with -demonware_debug; every AE and marketplace request passes here.
		static const auto enabled = utils::flags::has_flag("-demonware_debug");
		if (!enabled)
		{
			return;
		}

		static std::atomic_uint64_t sequence{};
		const auto path = std::string{"s2x/dump/dw/hq_"} + label + "_" +
			std::to_string(GetCurrentProcessId()) + "_" + std::to_string(sequence++) + ".bin";
		if (!utils::io::write_file(path, bytes)) console::error("[HQ protocol] cannot write %s\n", path.c_str());
		console::info("[HQ protocol] %s: %zu raw bytes at %s\n", label, bytes.size(), path.c_str());
	}

	// Per-row traces fire once for every catalog entry: one vendor visit wrote 1648 files
	// and 1648 console lines (run-39688), all synchronously on the Demonware thread while
	// the client was waiting on that same reply. Keep the first rows for schema work and
	// account for the rest, so a full catalog page cannot stall the task queue.
	inline void trace_row(const char* label, const std::string& bytes, const std::size_t keep = 4)
	{
		static const auto enabled = utils::flags::has_flag("-demonware_debug");
		if (!enabled) return;
		static std::mutex mutex{};
		static std::map<std::string, std::uint64_t> rows{};
		std::uint64_t index{};
		{
			std::lock_guard lock{mutex};
			index = rows[label]++;
		}
		if (index < keep) { trace(label, bytes); return; }
		if (index == keep) console::info("[HQ protocol] %s: dumping only the first %zu rows\n", label, keep);
		else if ((index + 1) % 512 == 0)
			console::info("[HQ protocol] %s: %llu rows served\n", label, index + 1);
	}
}
