#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/dvars.hpp"
#include "game/lookup/errors.hpp"

#include "game/scripting/functions.hpp"

#include "component/command.hpp"
#include "component/console/console.hpp"

#include "script_error.hpp"
#include "script_extension.hpp"
#include "script_loading.hpp"
#include "script_storage.hpp"

#include <utils/hook.hpp>
#include <utils/string.hpp>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace gsc
{
	std::uint16_t function_id_start = 0x3C6;

	namespace
	{
		struct gsc_error : public std::runtime_error
		{
			using std::runtime_error::runtime_error;
		};

		std::unordered_map<std::uint16_t, game::BuiltinFunction> functions;
		std::unordered_map<std::uint32_t, game::BuiltinFunction> builtin_funcs_overrides;

		bool force_error_print = false;
		std::optional<std::string> gsc_error_msg;

		std::vector<devmap_entry> devmap_entries{};

		constexpr std::uint32_t stock_function_count = 0x3C6;
		constexpr std::uint32_t max_custom_function_id = 0x1000;

		utils::hook::detour scr_error_hook;
		utils::hook::detour scr_error2_hook;

		std::string get_builtin_function_name(const std::uint32_t id)
		{
			try
			{
				return gsc_ctx->func_name(static_cast<std::uint16_t>(id));
			}
			catch (...)
			{
				return utils::string::va("#%u", id);
			}
		}

		std::optional<devmap_entry> get_devmap_entry(const std::uint8_t* codepos)
		{
			const auto itr = std::ranges::find_if(devmap_entries, [codepos](const devmap_entry& entry) -> bool
			{
				return codepos >= entry.bytecode && codepos < entry.bytecode + entry.size;
			});

			if (itr != devmap_entries.end())
			{
				return *itr;
			}

			return {};
		}

		std::optional<std::pair<std::uint16_t, std::uint16_t>> get_line_and_col_for_codepos(const std::uint8_t* codepos)
		{
			const auto entry = get_devmap_entry(codepos);

			if (!entry.has_value())
			{
				return {};
			}

			std::optional<std::pair<std::uint16_t, std::uint16_t>> best_line_info{};
			std::uint32_t best_codepos = 0;

			assert(codepos >= entry->bytecode);
			const std::uint32_t codepos_offset = static_cast<std::uint32_t>(codepos - entry->bytecode);

			for (const auto& instruction : entry->devmap)
			{
				if (instruction.codepos > codepos_offset)
				{
					continue;
				}

				if (best_line_info.has_value() && codepos_offset - instruction.codepos > codepos_offset - best_codepos)
				{
					continue;
				}

				best_line_info = { { instruction.line, instruction.col } };
				best_codepos = instruction.codepos;
			}

			return best_line_info;
		}

		void execute_custom_function(game::BuiltinFunction function)
		{
			auto error = false;

			try
			{
				function();
			}
			catch (const std::exception& ex)
			{
				error = true;
				force_error_print = true;
				gsc_error_msg = ex.what();
			}

			if (error)
			{
				game::Scr_ErrorInternal();
			}
		}

		void vm_call_builtin_function(const std::uint32_t index)
		{
			game::BuiltinFunction function = nullptr;
			bool custom_or_override = false;

			// Stock builtin override, e.g. print/assert/etc.
			if (const auto itr = builtin_funcs_overrides.find(index); itr != builtin_funcs_overrides.end())
			{
				function = itr->second;
				custom_or_override = true;
			}
			else if (index <= std::numeric_limits<std::uint16_t>::max())
			{
				if (const auto itrr = functions.find(static_cast<std::uint16_t>(index)); itrr != functions.end())
				{
					function = itrr->second;
					custom_or_override = true;
				}
			}

			// Stock S2 builtin.
			if (!function && index > 0 && index <= stock_function_count)
			{
				function = reinterpret_cast<game::BuiltinFunction>(scripting::get_function_by_index(index));
			}

			if (!function)
			{
				const auto name = get_builtin_function_name(index);
				scr_error(utils::string::va("builtin function \"%s\" doesn't exist", name.data()));
				return;
			}

			if (custom_or_override)
			{
				execute_custom_function(function);
			}
			else
			{
				function();
			}
		}

		void builtin_call_error(const std::string& error)
		{
			const auto pos = game::scr_function_stack->pos;
			const auto function_id = *reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::size_t>(pos - 2));

			if (function_id >= 0x8000)
			{
				console::warn("in call to builtin method \"%s\"%s\n", gsc_ctx->meth_name(function_id).data(), error.data());
			}
			else
			{
				console::warn("in call to builtin function \"%s\"%s\n", gsc_ctx->func_name(function_id).data(), error.data());
			}
		}

		std::optional<std::string> get_opcode_name(const std::uint8_t opcode)
		{
			try
			{
				const auto index = gsc_ctx->opcode_enum(opcode);
				return {gsc_ctx->opcode_name(index)};
			}
			catch (...)
			{
				return {};
			}
		}

		void print_callstack()
		{
			for (auto frame = game::scr_VmPub->function_frame; frame != game::scr_VmPub->function_frame_start; --frame)
			{
				const auto pos = frame == game::scr_VmPub->function_frame ? game::scr_function_stack->pos : frame->fs.pos;
				const auto function = find_function(frame->fs.pos);

				std::string location;
				if (function.has_value())
				{
					location = std::format("{}::{}", function->second, function->first);
				}
				else
				{
					location = std::format("unknown location {}", reinterpret_cast<const void*>(pos));
				}

				const auto line_info = get_line_and_col_for_codepos(reinterpret_cast<const std::uint8_t*>(pos));
				if (line_info.has_value())
				{
					location += std::format(":{}:{}", line_info->first, line_info->second);
				}

				console::warn("  at %s\n", location.data());
			}
		}

		void scr_error_stub(const char* fmt, ...)
		{
			const auto translated = game::lookup::errors::translate_format(fmt);
			char buffer[0x400]{};

			va_list args;
			va_start(args, fmt);
			vsnprintf(buffer, sizeof(buffer), translated.data(), args);
			va_end(args);

			buffer[sizeof(buffer) - 1] = '\0';

			if (!gsc_error_msg.has_value())
			{
				gsc_error_msg = buffer;
			}

			scr_error_hook.invoke<void>("%s", buffer);
		}

		void* scr_error2_stub(int param_index, const char* fmt, ...)
		{
			const auto translated = game::lookup::errors::translate_format(fmt);
			char buffer[0x400]{};

			va_list args;
			va_start(args, fmt);
			vsnprintf(buffer, sizeof(buffer), translated.data(), args);
			va_end(args);

			buffer[sizeof(buffer) - 1] = '\0';

			if (!gsc_error_msg.has_value())
			{
				gsc_error_msg = buffer;
			}

			return scr_error2_hook.invoke<void*>(param_index, "%s", buffer);
		}

		void vm_error_stub(uint64_t mark_pos)
		{
			if (dvars::developer_script->current.integer == 0 && !force_error_print)
			{
				game::LargeLocalResetToMark(mark_pos);
				return;
			}

			console::warn("******* script runtime error ********\n");
			const auto opcode_id = *reinterpret_cast<std::uint8_t*>(game::select(0xBB18F90, 0xAB9B190));

			const std::string error_suffix = gsc_error_msg.has_value() ? std::format(": {}", gsc_error_msg.value()) : std::string();

			if ((opcode_id >= 0x1A && opcode_id <= 0x20) || (opcode_id >= 0xA9 && opcode_id <= 0xAF))
			{
				builtin_call_error(error_suffix);
			}
			else
			{
				const auto opcode = get_opcode_name(opcode_id);
				if (opcode.has_value())
				{
					console::warn("while processing instruction %s%s\n", opcode.value().data(), error_suffix.data());
				}
				else
				{
					console::warn("while processing instruction 0x%X%s\n", opcode_id, error_suffix.data());
				}
			}

			print_callstack();

			force_error_print = false;
			gsc_error_msg = {};

			console::warn("************************************\n");
			game::LargeLocalResetToMark(mark_pos);
		}

		void scr_print()
		{
			for (auto i = 0u; i < game::Scr_GetNumParam(); ++i)
			{
				console::info("%s", game::Scr_GetString(i));
			}
		}

		void scr_print_ln()
		{
			for (auto i = 0u; i < game::Scr_GetNumParam(); ++i)
			{
				console::info("%s", game::Scr_GetString(i));
			}

			console::info("\n");
		}

		void scr_file_read()
		{
			if (game::Scr_GetNumParam() != 1) throw std::runtime_error("fileread(filename) expects one argument");
			const auto text = script_storage::read(game::get_appdata_path() / "scriptdata", game::Scr_GetString(0));
			if (text) game::Scr_AddString(text->c_str()); // Missing files return undefined; empty files return "".
		}

		void scr_file_write()
		{
			if (game::Scr_GetNumParam() != 2) throw std::runtime_error("filewrite(filename, text) expects two arguments");
			const std::string name{game::Scr_GetString(0)};
			const std::string text{game::Scr_GetString(1)};
			script_storage::write(game::get_appdata_path() / "scriptdata", name, text);
			game::Scr_AddInt(1);
		}

		void scr_get_ip()
		{
			if (game::Scr_GetNumParam() != 1 || game::Scr_GetType(0) != game::VAR_POINTER ||
				game::Scr_GetPointerType(0) != game::VAR_ENTITY)
				throw std::runtime_error("getip(player) expects a player entity");
			const auto ref = game::Scr_GetEntityIdRef(game::Scr_GetObject(0));
			const auto* clients = *game::mp::svs_clients;
			if (ref.classnum || !game::SV_Loaded() || !clients || ref.entnum >= *game::sv_maxclients ||
				clients[ref.entnum].state < 3 || !clients[ref.entnum].gentity || !clients[ref.entnum].gentity->client)
				throw std::runtime_error("getip(player) requires a connected player");
			const auto& address = clients[ref.entnum].remoteAddress;
			if (address.type == game::NA_LOOPBACK) game::Scr_AddString("127.0.0.1");
			else if (address.type == game::NA_IP)
				game::Scr_AddString(utils::string::va("%u.%u.%u.%u", address.ip[0], address.ip[1], address.ip[2], address.ip[3]));
			else game::Scr_AddString(""); // Bots and unavailable addresses have no public IP.
		}

		void scr_get_player_roster()
		{
			if (game::Scr_GetNumParam()) throw std::runtime_error("getplayerroster() expects no arguments");
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> json{buffer};
			json.StartObject();
			json.Key("generated_utc"); json.Int64(std::time(nullptr));
			json.Key("mapname"); json.String(game::Dvar_GetString("mapname"));
			json.Key("players"); json.StartArray();
			const auto* clients = *game::mp::svs_clients;
			if (game::SV_Loaded() && clients)
			{
				for (auto index = 0; index < *game::sv_maxclients; ++index)
				{
					const auto& client = clients[index];
					if (client.state < 3 || client.testClient || client.remoteAddress.type == game::NA_BOT || !client.guid[0]) continue;
					json.StartObject();
					json.Key("id"); json.String(client.guid, static_cast<rapidjson::SizeType>(strnlen(client.guid, sizeof(client.guid))));
					json.Key("name"); json.String(client.name, static_cast<rapidjson::SizeType>(strnlen(client.name, sizeof(client.name))));
					json.EndObject();
				}
			}
			json.EndArray(); json.EndObject();
			game::Scr_AddString(buffer.GetString());
		}

	}

	void add_devmap_entry(std::uint8_t* codepos, std::size_t size, const std::string& name, xsk::gsc::buffer devmap_buf)
	{
		std::vector<dev_map_instruction> devmap{};
		const auto* devmap_ptr = reinterpret_cast<const dev_map*>(devmap_buf.data);

		devmap.resize(devmap_ptr->num_instructions);
		std::memcpy(devmap.data(), devmap_ptr->instructions, sizeof(dev_map_instruction) * devmap_ptr->num_instructions);

		devmap_entries.emplace_back(codepos, size, name, std::move(devmap));
	}

	void clear_devmap()
	{
		devmap_entries.clear();
	}

	void scr_error(const char* error)
	{
		force_error_print = true;
		gsc_error_msg = error;

		game::Scr_ErrorInternal();
	}

	void override_function(const std::string& name, game::BuiltinFunction func)
	{
		const auto id = gsc_ctx->func_id(name);
		builtin_funcs_overrides[id] = func;
	}

	void add_function(const std::string& name, game::BuiltinFunction function)
	{
		if (gsc_ctx->func_exists(name))
		{
			const auto id = gsc_ctx->func_id(name);
			builtin_funcs_overrides[id] = function;
			return;
		}

		if (function_id_start >= max_custom_function_id)
		{
			throw std::runtime_error("custom GSC function id limit exceeded");
		}

		const auto id = ++function_id_start;

		functions[id] = function;
		gsc_ctx->func_add(name, id);
	}

	void notify_chat(const int client_num, const std::string& text, const bool team)
	{
		if (text.empty() || !game::SV_Loaded() || game::virtual_lobby_loaded() ||
			client_num < 0 || client_num >= *game::sv_maxclients) return;
		auto* entities = game::mp::g_entities.get();
		if (!entities || !entities[client_num].client) return;
		// Intern only for this notification: map unload may release all script strings.
		// These MP helpers are SL_GetString and RemoveRefToValue (VAR_STRING).
		const auto event = utils::hook::invoke<unsigned>(0x6891F0_g, "s2x_chat", 0u);
		game::Scr_AddInt(team ? 1 : 0);
		game::Scr_AddString(text.c_str());
		game::Scr_Notify(&entities[client_num], event, 2);
		utils::hook::invoke<void>(0x68B4A0_g, static_cast<int>(game::VAR_STRING), static_cast<std::uint64_t>(event));
	}

	class extension final : public generic_component
	{
	public:
		void post_unpack() override
		{
			override_function("print", &scr_print);
			override_function("println", &scr_print_ln);
			if (game::environment::uses_multiplayer_binary())
			{
				add_function("fileread", &scr_file_read);
				add_function("filewrite", &scr_file_write);
				add_function("getip", &scr_get_ip);
				add_function("getplayerroster", &scr_get_player_roster);
			}

			scr_error_hook.create(game::Scr_Error, scr_error_stub);
			scr_error2_hook.create(game::Scr_Error2, scr_error2_stub);

			utils::hook::nop(game::select(0x6939DC, 0x49A28C), 14);
			utils::hook::call(game::select(0x6939DC, 0x49A28C), vm_call_builtin_function);

			utils::hook::call(game::select(0x694BDC, 0x49B48C), vm_error_stub); // LargeLocalResetToMark
		}
	};
}

REGISTER_COMPONENT(gsc::extension)
