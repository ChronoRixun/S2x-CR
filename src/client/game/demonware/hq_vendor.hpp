#pragma once

#include "hq_protocol.hpp"

namespace demonware::hq_vendor
{
	inline std::atomic_uint32_t requests{}, replies{}, rejected{};

	// Provisional transaction/store acknowledgement. Field meanings in the request
	// are captured; the native read-side result schema is still unverified.
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
		if (fields[0] != "s2_steam" || fields[1].size() != 36 || fields[2].empty() || fields[2].size() > 32 ||
			request.substr(at) != std::string("\x20\x01", 2)) return false;
		response.clear();
		// Hypothesis for this struct: transaction first, store second (not recovered).
		response += '\x0A'; response += static_cast<char>(fields[2].size()); response += fields[2];
		response += '\x12'; response += static_cast<char>(fields[1].size()); response += fields[1];
		return true;
	}

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
