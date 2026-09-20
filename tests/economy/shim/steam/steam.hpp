#pragma once
namespace steam {
struct id { unsigned long long bits{42}; };
struct user { id GetSteamID() { return {}; } };
inline user* SteamUser() { static user result; return &result; }
}
