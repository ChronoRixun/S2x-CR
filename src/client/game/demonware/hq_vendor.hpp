#pragma once

#include "hq_protocol.hpp"

namespace demonware::hq_vendor
{
	inline std::atomic_uint32_t requests{}, replies{}, rejected{};

	// Task 242 is applyConversionRule. Native response reader A4C850 expects
	// transaction string, uint64, rule object, then repeated currency/item records.
	inline bool reply_body(const std::string& request, std::string& response)
	{
		std::size_t at{};
		std::string fields[3];
		for (unsigned i = 0; i < 3; ++i)
		{
			if (at + 2 > request.size() || static_cast<unsigned char>(request[at++]) != (i + 1) * 8 + 2) return false;
			const auto size = static_cast<unsigned char>(request[at++]);
			if (size > 64 || size > request.size() - at) return false;
			fields[i] = request.substr(at, size);
			at += size;
			if (fields[i].find('\0') != std::string::npos) return false;
		}
		if (fields[0] != "s2_steam" || fields[1] != "3cf6ce39-7313-4bd0-1fcf-c8ba7b0eecd6" || fields[2].empty() || fields[2].size() > 24 ||
			request.substr(at) != std::string("\x20\x01", 2)) return false;
		response.clear();
		// Known startup rule only: acknowledge without inventing conversion rewards.
		// Scalar semantics are provisional; field types/limits are native-confirmed.
		response += '\x0A'; response += static_cast<char>(fields[2].size()); response += fields[2];
		response.append("\x10\x00", 2);
		std::string rule;
		rule += '\x0A'; rule += static_cast<char>(fields[0].size()); rule += fields[0];
		rule.append("\x12\x00", 2); // unknown display/name string
		rule += '\x1A'; rule += static_cast<char>(fields[1].size()); rule += fields[1];
		rule.append("\x20\x01", 2);
		response += '\x1A'; response += static_cast<char>(rule.size()); response += rule;
		// Fields 4..6 are absent, i.e. zero repeated records. Native loops use counts.

		return true;
	}

	// Read-side chain A4A5A0 -> A4A510 -> A4A2C0, native stride 0x370.
	// One local display offer: table supplydroptypes.csv sd_mp, item GUID 1.
	// SKU/product ID and price are local policy; purchasing is unsupported.
	class catalog_result final : public bdTaskResult
	{
	public:
		void serialize(byte_buffer* buffer) override
		{
			buffer->write_uint32(1); // +20 SKU ID
			buffer->write_uint32(1); // +24 product ID
			buffer->write_ubyte(1); // +28
			buffer->write_blob(std::string{"sd_mp", 6}); // +29 bounded SKU data (64 bytes)
			buffer->write_ubyte(1); // +6A
			buffer->write_uint32(0); // +6C
			buffer->write_uint32(0); // +70 sale end
			buffer->write_uint32(0); // +74
			buffer->write_ubyte(0); // +78
			buffer->write_blob(std::string(1, '\0')); // +80 promotional text (135 bytes)
			buffer->write_uint32(0); // +10C
			buffer->write_uint16(0); // +110
			buffer->write_uint32(0); // +114
			buffer->write_ubyte(0); // +6B
			buffer->write_uint32(1); // +118 price count, fixed native capacity 10
			buffer->write_ubyte(2); // price +20 currency ID
			buffer->write_uint32(200); // price +24 absolute price
			buffer->write_ubyte(100); // +350 SKU type, excludes collection type150
			buffer->write_uint32(1); // +358 max quantity
			buffer->write_bool(false); // +35C sold out
		}
	};

	class result final : public bdTaskResult
	{
	public:
		std::string body;
		void serialize(byte_buffer* buffer) override
		{
			buffer->write_struct(body.data(), static_cast<int>(body.size()));
		}
	};
}
