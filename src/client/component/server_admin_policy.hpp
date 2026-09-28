#pragma once
#include <string>
#include <string_view>
#include <chrono>
#include <atomic>

namespace server_admin::policy
{
    inline bool frame_length(const unsigned int size) { return size > 0 && size <= 8192; }
    inline bool hex(const char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
    inline bool nonce(const std::string_view value)
    { if (value.size() != 32) return false; for (const auto c : value) if (!hex(c)) return false; return true; }
    inline bool request_id(const std::string_view value)
    {
        if (value.size() != 36) return false;
        for (size_t i = 0; i < value.size(); ++i)
        { if (i == 8 || i == 13 || i == 18 || i == 23) { if (value[i] != '-') return false; } else if (!hex(value[i])) return false; }
        return true;
    }
    inline bool operation(const std::string_view value)
    { return value == "hello" || value == "players" || value == "announce" || value == "warn" || value == "kick"; }
    // UTF8 validation and control exclusion; limit applies to encoded bytes, not UTF16 code units.
    inline bool message(const std::string_view value)
    {
        if (value.empty() || value.size() > 160) return false;
        for (size_t at = 0; at < value.size();)
        {
            auto first = static_cast<unsigned char>(value[at++]);
            unsigned int cp = first, extra = 0, minimum = 0;
            if (first < 0x80) {}
            else if (first >= 0xc2 && first <= 0xdf) { cp = first & 31; extra = 1; minimum = 0x80; }
            else if (first >= 0xe0 && first <= 0xef) { cp = first & 15; extra = 2; minimum = 0x800; }
            else if (first >= 0xf0 && first <= 0xf4) { cp = first & 7; extra = 3; minimum = 0x10000; }
            else return false;
            if (at + extra > value.size()) return false;
            for (unsigned int i = 0; i < extra; ++i)
            { auto c = static_cast<unsigned char>(value[at++]); if ((c & 0xc0) != 0x80) return false; cp = (cp << 6) | (c & 63); }
            if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) || cp < 32 || (cp >= 127 && cp <= 159) || cp == 0x2028 || cp == 0x2029) return false;
        }
        return true;
    }
    template <typename Guid, typename Address>
    inline bool same_connection(const Guid& expected_guid, const Guid& current_guid,
        const Address& expected_address, const Address& current_address, int expected_qport,
        int current_qport, int expected_time, int current_time)
    { return expected_guid == current_guid && expected_address == current_address && expected_qport == current_qport && expected_time == current_time; }
    inline bool target_allowed(bool is_bot, bool is_host, bool has_guid)
    { return !is_bot && !is_host && has_guid; }
    enum class work_state { pending, running, cancelled, complete };
    inline bool begin(std::atomic<work_state>& state, const std::chrono::steady_clock::time_point deadline)
    {
        if (std::chrono::steady_clock::now() >= deadline) { auto expected = work_state::pending; state.compare_exchange_strong(expected, work_state::cancelled); return false; }
        auto expected = work_state::pending;
        return state.compare_exchange_strong(expected, work_state::running);
    }
    inline void cancel(std::atomic<work_state>& state)
    { auto expected = work_state::pending; state.compare_exchange_strong(expected, work_state::cancelled); }
}
