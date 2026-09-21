"""Compile the production chat routing/notification functions with native-call stubs."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/tests/chat'
out.mkdir(parents=True, exist_ok=True)


def extract(path, signature, indentation):
    source = (root / path).read_text()
    start = source.index(signature)
    end = source.index('\n' + indentation + '}', start) + len(indentation) + 2
    return source[start:end]


notify = extract('src/client/component/gsc/script_extension.cpp', 'void notify_chat(', '\t')
route = extract('src/client/component/command.cpp', 'void client_command_mp_stub(', '\t\t')
code = r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#ifdef NDEBUG
#error Tests require assertions
#endif
constexpr uint64_t operator""_g(unsigned long long value) { return value; }
bool loaded = true, lobby{}, handled{}, mutate{};
int native_calls{}, notifications{}, references{}, team_seen{};
std::string text_seen;
std::vector<std::string> tokens, stack;
namespace game {
constexpr int VAR_STRING = 2;
int limit = 4; int* sv_maxclients = &limit;
struct entity { bool client = true; } entities[4];
namespace mp { struct symbol { entity* get() { return entities; } } g_entities; }
bool SV_Loaded() { return loaded; }
bool virtual_lobby_loaded() { return lobby; }
void Scr_AddInt(int value) { stack.push_back(std::to_string(value)); }
void Scr_AddString(const char* value) { stack.push_back(value); }
void Scr_Notify(entity*, unsigned event, unsigned count) {
 assert(event == 7 && count == 2 && stack.size() == 2);
 team_seen = std::stoi(stack[0]); text_seen = stack[1]; stack.clear(); ++notifications;
}
}
namespace utils::hook {
template<class T, class... Args> T invoke(uint64_t address, Args...) {
 if constexpr (std::is_void_v<T>) { assert(address == 0x68B4A0); --references; }
 else { assert(address == 0x6891F0); ++references; return 7; }
}
}
namespace gsc { NOTIFY }
struct params_sv {
 const char* operator[](int i) const { return i < static_cast<int>(tokens.size()) ? tokens[i].c_str() : ""; }
 std::string join(int first) const { std::string s; for (int i = first; i < static_cast<int>(tokens.size()); ++i) { if (i > first) s += ' '; s += tokens[i]; } return s; }
};
bool execute_custom_sv_command_internal(int, const params_sv&) { return handled; }
struct native_hook {
 template<class T> void invoke(int) { ++native_calls; if (mutate) tokens = {"changed", "different payload"}; }
} client_command_mp_hook;
ROUTE
int main() {
 tokens = {"say", "hello", "world"}; client_command_mp_stub(1);
 assert(native_calls == 1 && notifications == 1 && text_seen == "hello world" && team_seen == 0 && references == 0);
 tokens = {"say_team", "team only"}; mutate = true; client_command_mp_stub(2); mutate = false;
 assert(native_calls == 2 && notifications == 2 && text_seen == "team only" && team_seen == 1 && references == 0);
 tokens = {"say", std::string(400, 'a')}; client_command_mp_stub(1);
 assert(text_seen.size() == 256 && notifications == 3);
 for (const auto verb : {"kill", "sayteam", "say_team_extra", ""}) { tokens = {verb, "text"}; client_command_mp_stub(1); }
 assert(notifications == 3);
 tokens = {"say", ""}; client_command_mp_stub(1); assert(notifications == 3);
 tokens = {"say", "text"}; handled = true; const auto calls = native_calls; client_command_mp_stub(1);
 assert(native_calls == calls && notifications == 3); handled = false;
 for (const int id : {-1, 4, 999}) client_command_mp_stub(id);
 loaded = false; client_command_mp_stub(1); loaded = true;
 lobby = true; client_command_mp_stub(1); lobby = false;
 game::entities[1].client = false; client_command_mp_stub(1);
 assert(notifications == 3 && references == 0 && stack.empty());
 std::cout << "PASS: production chat routing, payload ownership, team flag, bounds, context guards and string references\n";
}
'''.replace('NOTIFY', notify).replace('ROUTE', route)
(out / 'chat.cpp').write_text(code)
project = (root / 'tests/scripting/storage.vcxproj').read_text()
project = project.replace('$(ProjectDir)..\\..\\build\\tests\\scripting\\bin\\', '$(ProjectDir)bin\\')
project = project.replace('$(ProjectDir)..\\..\\build\\tests\\scripting\\obj\\', '$(ProjectDir)obj\\')
project = project.replace('storage.cpp', 'chat.cpp')
(out / 'chat.vcxproj').write_text(project)
msbuild = r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
subprocess.run([msbuild, str(out / 'chat.vcxproj'), '/p:Configuration=Release', '/p:Platform=x64',
                '/m:1', '/v:minimal', '/nologo'], env={k.upper(): v for k, v in os.environ.items()}, check=True)
subprocess.run([str(out / 'bin/chat.exe')], check=True)
