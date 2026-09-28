#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "server_admin.hpp"
#include "network.hpp"
#include "server_admin_policy.hpp"
#include "server_admin_pipe.hpp"
#include "scheduler.hpp"
#include "network.hpp"
#include "gsc/script_extension.hpp"
#include "console/console.hpp"
#include "game/game.hpp"
#include <utils/flags.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sddl.h>
#include <bcrypt.h>
#include <condition_variable>
#include <deque>
#include <future>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace server_admin
{
    namespace
    {
        using clock = std::chrono::steady_clock;
        std::atomic_bool active{}, notice_ready{}, stopping{};
        std::atomic_uint64_t map_epoch{};
        std::atomic_uint queued_work{};
        std::string instance;
        HANDLE stop_event{};
        std::thread worker;
        game::dvar_t* ready_dvar{};
        game::dvar_t* notice_dvar{};

        using pipe_io::handle;
        struct request { std::string id, operation, target, message; };
        struct row { int slot{}; std::string name, token, state; bool bot{}, host{}; };
        struct reply
        {
            bool ok{};
            std::string code, message;
            int recipients{};
            std::vector<row> players;
        };
        struct identity
        {
            int slot{};
            decltype(std::to_array(game::mp::client_t{}.guid)) guid{};
            game::netadr_s address{};
            int qport{}, connected{};
            std::uint64_t epoch{};
            clock::time_point expires{};
        };
        // This registry is accessed exclusively by scheduler::server callbacks.
        std::unordered_map<std::string, identity> identities;

        std::string random_token()
        {
            unsigned char bytes[16]{};
            if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
                throw std::runtime_error("token generation failed");
            std::string token; token.reserve(32);
            for (auto b : bytes) { token.push_back("0123456789abcdef"[b >> 4]); token.push_back("0123456789abcdef"[b & 15]); }
            return token;
        }
        std::string clean_name(const game::mp::client_t& client)
        {
            std::string name;
            for (size_t i = 0; i < sizeof(client.name) && client.name[i]; ++i)
            {
                auto c = static_cast<unsigned char>(client.name[i]);
                if (c == '^' && i + 1 < sizeof(client.name) && client.name[i + 1] >= '0' && client.name[i + 1] <= ';') { ++i; continue; }
                if (c >= 32 && c != 127) name.push_back(client.name[i]);
            }
            if (name.empty() || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), nullptr, 0)) return "Player";
            return name;
        }
        reply failure(const char* code, const char* message) { return {false, code, message}; }
        bool bot(const game::mp::client_t& c) { return c.testClient || c.remoteAddress.type == game::NA_BOT; }
        bool host(const game::mp::client_t& c) { return c.remoteAddress.type == game::NA_LOOPBACK; }
        bool matches(const identity& id, const game::mp::client_t& c)
        {
            return c.state > 1 && id.epoch == map_epoch.load() && clock::now() < id.expires &&
                policy::same_connection(id.guid, std::to_array(c.guid), id.address, c.remoteAddress, id.qport, c.qport, id.connected, c.lastConnectTime);
        }
        reply execute(const request& req)
        {
            if (stopping || !active) return failure("stopping", "Server is shutting down.");
            if (!game::is_server_running()) return failure("no_match", "A server match must be running.");
            auto* clients = *game::mp::svs_clients;
            const int count = std::clamp(*game::sv_maxclients, 0, 18);
            if (!clients) return failure("no_match", "Player registry is unavailable.");
            if (req.operation == "players")
            {
                identities.clear();
                reply result{true, "ok", "Current connected players."};
                for (int slot = 0; slot < count; ++slot)
                {
                    auto& c = clients[slot];
                    if (c.state <= 1) continue;
                    auto token = random_token();
                    identities.emplace(token, identity{slot, std::to_array(c.guid), c.remoteAddress,
                        c.qport, c.lastConnectTime, map_epoch.load(), clock::now() + 30s});
                    result.players.push_back({slot, clean_name(c), token, c.state >= 5 ? "playing" : "connecting", bot(c), host(c)});
                }
                return result;
            }
            if (req.operation == "announce")
            {
                if (!notice_ready) return failure("notice_unavailable", "The notice script is not ready in this match.");
                reply result{true, "ok", "Notice queued to ready player listeners."};
                for (int slot = 0; slot < count; ++slot)
                    if (clients[slot].state >= 5 && !bot(clients[slot]) && !host(clients[slot]) && gsc::notify_admin_notice(slot, req.message, false)) ++result.recipients;
                if (!result.recipients) return failure("no_recipients", "No ready human notice listeners.");
                console::info("[server_admin] announcement id=%s recipients=%d\n", req.id.c_str(), result.recipients);
                return result;
            }
            auto found = identities.find(req.target);
            if (found == identities.end() || found->second.slot >= count || !matches(found->second, clients[found->second.slot]))
                return failure("stale_target", "The connection changed or the roster expired. Refresh players.");
            auto& target = clients[found->second.slot];
            if (!policy::target_allowed(bot(target), host(target), target.guid[0] != 0)) return failure("protected_target", "Bots, local hosts and incomplete identities cannot be moderated.");
            if (req.operation == "warn")
            {
                if (!notice_ready || target.state < 5 || !gsc::notify_admin_notice(found->second.slot, req.message, true))
                    return failure("notice_unavailable", "The target has no ready notice listener.");
                console::info("[server_admin] warning id=%s slot=%d\n", req.id.c_str(), found->second.slot);
                return {true, "ok", "Warning queued to the player's script listener.", 1};
            }
            // Fixed non-blacklisting kick. Never interpolate a reason into console/script commands.
            game::mp::SV_DropClient(&target, "EXE_PLAYERKICKED", 1);
            target.lastPacketTime = *game::mp::svs_time;
            console::info("[server_admin] kick id=%s slot=%d reason=%s\n", req.id.c_str(), found->second.slot, req.message.c_str());
            identities.erase(found);
            return {true, "ok", "Player disconnected.", 1};
        }
        struct work
        {
            std::atomic<policy::work_state> state{policy::work_state::pending};
            clock::time_point deadline{clock::now() + 2s};
            std::promise<reply> completion;
        };
        reply dispatch(const request& req)
        {
            if (req.operation == "hello") return {true, "ok", "Local manager administration is available."};
            // A lobby without server frames must not accumulate unbounded expired callbacks.
            if (queued_work.fetch_add(1) >= 8)
            { --queued_work; return failure("busy", "The server callback queue is full. Wait for an active match."); }
            auto item = std::make_shared<work>();
            auto future = item->completion.get_future();
            scheduler::once([item, req]
            {
                --queued_work;
                if (stopping || !policy::begin(item->state, item->deadline)) return;
                try { item->completion.set_value(execute(req)); }
                catch (const std::exception&) { item->completion.set_value(failure("internal_error", "Administration failed without a confirmed result.")); }
                item->state = policy::work_state::complete;
            }, scheduler::server);
            while (clock::now() < item->deadline && !stopping)
                if (future.wait_for(20ms) == std::future_status::ready) return future.get();
            policy::cancel(item->state);
            if (item->state == policy::work_state::running || item->state == policy::work_state::complete)
                return failure("result_unknown", "The action started but its result was not received. Do not retry automatically.");
            return failure("expired", "The server did not process the request before its deadline; no action was started.");
        }
        std::string serialize(const request& req, const reply& response)
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> out(buffer);
            out.StartObject();
            out.Key("version"); out.Int(1); out.Key("id"); out.String(req.id.c_str());
            out.Key("ok"); out.Bool(response.ok); out.Key("code"); out.String(response.code.c_str());
            out.Key("message"); out.String(response.message.c_str()); out.Key("instance"); out.String(instance.c_str());
            out.Key("pid"); out.Uint(GetCurrentProcessId()); out.Key("noticeReady"); out.Bool(notice_ready.load());
            out.Key("recipients"); out.Int(response.recipients); out.Key("players"); out.StartArray();
            for (const auto& player : response.players)
            {
                out.StartObject(); out.Key("slot"); out.Int(player.slot); out.Key("name"); out.String(player.name.c_str());
                out.Key("token"); out.String(player.token.c_str()); out.Key("isBot"); out.Bool(player.bot);
                out.Key("isHost"); out.Bool(player.host); out.Key("state"); out.String(player.state.c_str()); out.EndObject();
            }
            out.EndArray(); out.EndObject();
            return {buffer.GetString(), buffer.GetSize()};
        }
        bool parse(const std::string& payload, request& req)
        {
            rapidjson::Document doc;
            doc.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(payload.data(), payload.size());
            if (doc.HasParseError() || !doc.IsObject() || doc.MemberCount() > 7) return false;
            std::unordered_set<std::string> keys;
            for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
                if (!keys.emplace(it->name.GetString(), it->name.GetStringLength()).second) return false;
            auto text = [&](const char* key) -> std::string
            { if (!doc.HasMember(key) || !doc[key].IsString()) return {}; return {doc[key].GetString(), doc[key].GetStringLength()}; };
            req.id = text("id"); req.operation = text("operation"); req.target = text("target"); req.message = text("message");
            if (!doc.HasMember("version") || !doc["version"].IsInt() || doc["version"].GetInt() != 1 || text("instance") != instance ||
                !policy::request_id(req.id) || !policy::operation(req.operation)) return false;
            if ((req.operation == "warn" || req.operation == "kick") && !policy::nonce(req.target)) return false;
            if ((req.operation == "warn" || req.operation == "announce") && !policy::message(req.message)) return false;
            if (req.operation == "kick" && !req.message.empty() && !policy::message(req.message)) return false;
            return true;
        }
        void serve()
        {
            try
            {
                PSECURITY_DESCRIPTOR descriptor{};
                if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(pipe_io::security_descriptor().c_str(), SDDL_REVISION_1, &descriptor, nullptr))
                    throw std::runtime_error("pipe DACL creation failed");
                const std::unique_ptr<void, decltype(&LocalFree)> release_descriptor(descriptor, &LocalFree);
                SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, false};
                const auto name = L"\\\\.\\pipe\\S2x.ServerAdmin." + std::wstring(instance.begin(), instance.end());
                std::deque<std::pair<std::string, std::string>> completed;
                while (!stopping)
                {
                    handle pipe(CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 8196, 8196, 0, &security));
                    if (pipe.value == INVALID_HANDLE_VALUE) throw std::runtime_error("pipe creation failed (name may already be owned)");
                    handle event(CreateEventW(nullptr, true, false, nullptr));
                    if (!event.value) throw std::runtime_error("pipe event creation failed");
                    OVERLAPPED operation{}; operation.hEvent = event.value;
                    DWORD bytes{};
                    BOOL connected = ConnectNamedPipe(pipe.value, &operation);
                    if (!connected)
                    {
                        const auto error = GetLastError();
                        if (error == ERROR_IO_PENDING) connected = pipe_io::wait_io(pipe.value, operation, bytes, clock::now() + 24h, stop_event);
                        else connected = error == ERROR_PIPE_CONNECTED;
                    }
                    if (!connected) { if (stopping) break; continue; }
                    std::uint32_t length{};
                    if (!pipe_io::transfer(pipe.value, &length, sizeof(length), false, stop_event, stopping) || !policy::frame_length(length)) continue;
                    std::string payload(length, '\0');
                    if (!pipe_io::transfer(pipe.value, payload.data(), length, false, stop_event, stopping)) continue;
                    request req;
                    std::string response;
                    if (!parse(payload, req))
                    {
                        if (!policy::request_id(req.id)) req.id.clear();
                        response = serialize(req, failure("invalid_request", "Invalid protocol, operation, instance, target or message (160 UTF8 bytes maximum; no controls)."));
                    }
                    else
                    {
                        auto prior = std::find_if(completed.begin(), completed.end(), [&](const auto& cached) { return cached.first == req.id; });
                        if (prior != completed.end()) response = prior->second;
                        else
                        {
                            response = serialize(req, dispatch(req));
                            completed.emplace_back(req.id, response);
                            if (completed.size() > 128) completed.pop_front();
                        }
                    }
                    if (response.size() > 8192) response = serialize(req, failure("response_too_large", "Response exceeds protocol bounds."));
                    length = static_cast<std::uint32_t>(response.size());
                    if (pipe_io::transfer(pipe.value, &length, sizeof(length), true, stop_event, stopping)) pipe_io::transfer(pipe.value, response.data(), length, true, stop_event, stopping);
                    // No FlushFileBuffers: a stalled client must never block process shutdown.
                    // Keep the connection alive until the reader closes, preserving unread reply bytes.
                    // Bounded overlapped read also permits immediate shutdown cancellation.
                    char ignored{};
                    pipe_io::transfer(pipe.value, &ignored, 1, false, stop_event, stopping);
                    DisconnectNamedPipe(pipe.value);
                }
            }
            catch (const std::exception& error)
            { active = false; console::warn("[server_admin] disabled: %s\n", error.what()); }
        }
        class component final : public multiplayer_component
        {
        public:
            void post_unpack() override
            {
                if (!game::environment::is_dedicated()) return;
                auto value = utils::flags::get_value("-server-manager-admin");
                if (!value || !policy::nonce(*value)) return;
                instance = *value;
                stop_event = CreateEventW(nullptr, true, false, nullptr);
                if (!stop_event) return;
                active = true; stopping = false;
                ready_dvar = game::Dvar_RegisterBool("s2x_server_admin_ready", true, game::DVAR_FLAG_READ);
                notice_dvar = game::Dvar_RegisterBool("s2x_server_admin_notice_ready", false, game::DVAR_FLAG_READ);
                worker = std::thread(serve);
            }
            void pre_destroy() override
            {
                stopping = true; active = false; notice_ready = false;
                if (stop_event) SetEvent(stop_event);
                if (worker.joinable()) worker.join();
                if (stop_event) { CloseHandle(stop_event); stop_event = nullptr; }
            }
        };
    }
    bool enabled() { return active.load() && !stopping.load(); }
    void set_notice_ready(const bool ready)
    {
        if (!ready) ++map_epoch;
        notice_ready = ready && enabled();
        if (notice_dvar) notice_dvar->current.enabled = notice_ready.load();
        if (ready_dvar) ready_dvar->current.enabled = enabled();
    }
}
REGISTER_COMPONENT(server_admin::component)
