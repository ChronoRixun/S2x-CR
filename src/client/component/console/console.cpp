#include <std_include.hpp>
#include "console.hpp"

#include "game/game.hpp"

#include "component/scheduler.hpp"

#include <utils/flags.hpp>

#include "terminal.hpp"
#include "syscon.hpp"

#include <utils/io.hpp>

namespace game_console
{
	void print(int type, const std::string& data);
}

namespace console
{
	enum console_type
	{
		con_type_none,
		con_type_terminal,
		con_type_syscon,
		con_type_game = con_type_syscon,
		con_type_default = con_type_syscon,
	} con_type;

	game::dvar_t* console_log = nullptr;

	void init_console_type()
	{
		con_type = con_type_default;

		const auto noconsole_flag = utils::flags::has_flag("-noconsole");
		if (!game::environment::is_dedicated() && noconsole_flag)
		{
			con_type = con_type_none;
			return;
		}

		const auto terminal_flag = utils::flags::has_flag("-terminal");
		if (terminal_flag)
		{
			con_type = con_type_terminal;
			return;
		}

		const auto syscon_flag = utils::flags::has_flag("-syscon");
		if (syscon_flag)
		{
			con_type = con_type_syscon;
			return;
		}
	}

	auto get_console_type()
	{
		return con_type;
	}

	bool is_enabled()
	{
		return get_console_type() != console_type::con_type_none;
	}

	namespace terminal
	{
		bool is_enabled()
		{
			return get_console_type() == console_type::con_type_terminal;
		}
	}

	namespace syscon
	{
		bool is_enabled()
		{
			return get_console_type() == console_type::con_type_syscon;
		}
	}

	std::string format(va_list* ap, const char* message)
	{
		static thread_local char buffer[0x1000];

		// _TRUNCATE hands back -1 when the line did not fit (with sizeof(buffer) as
		// the count the CRT would fail-fast instead). The terminated prefix is
		// still the diagnostic, so keep it and mark the cut. The cut swallowed the
		// line's own newline, so one is put back for every sink, not only the log
		// file, which is the only one that adds its own.
		const auto count = _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, message, *ap);
		if (count < 0)
		{
			std::string truncated{buffer, strnlen(buffer, sizeof(buffer) - 1)};
			if (!truncated.empty())
			{
				truncated += " [truncated]\n";
			}

			return truncated;
		}

		return { buffer, static_cast<size_t>(count) };
	}

	namespace
	{
		std::atomic<std::uint64_t> printed_lines{};
	}

	std::uint64_t lines_printed()
	{
		return printed_lines.load();
	}

	void dispatch_message(const int type, const std::string& message)
	{
		++printed_lines;
		std::string out = message;
		if (out.empty() || out.back() != '\n')
		{
			out.push_back('\n');
		}

		// A path given on the command line (+set g_consoleLog <path>, how the launcher
		// gives each server its own file) wins over the dvar: the dvar is registered
		// late and saved, so the native +set never reaches it.
		static const auto flag_path = utils::flags::get_set_value("g_consoleLog");
		if (flag_path)
			utils::io::write_file(*flag_path, out, true);
		else if (console_log && console_log->current.string)
			utils::io::write_file(console_log->current.string, out, true);

		if (console::is_enabled())
		{
			if (terminal::is_enabled())
			{
				::terminal::dispatch_message(type, message);
				return;
			}
			else if (syscon::is_enabled())
			{
				::syscon::Sys_Print(message.data());
			}
		}

		game_console::print(type, message);
	}

	void print(const int type, const char* fmt, ...)
	{
		if (type == console::print_type_demonware)
		{
			static bool has_demonware_debug = utils::flags::has_flag("-demonware_debug");
			if (!has_demonware_debug)
			{
				return;
			}
		}

		va_list ap;
		va_start(ap, fmt);
		const auto result = format(&ap, fmt);
		va_end(ap);

		dispatch_message(type, result);
	}

	void set_title(const std::string& title)
	{
		if (console::is_enabled())
		{
			if (terminal::is_enabled())
			{
				SetConsoleTitleA(title.data());
			}
			else if (syscon::is_enabled())
			{
				::syscon::set_title(title);
			}
		}
	}

	void init()
	{
		static auto initialized = false;
		if (initialized) return;
		initialized = true;

		init_console_type();
		if (get_console_type() == con_type_none)
		{
			return;
		}
		else if (get_console_type() == con_type_terminal)
		{
			::terminal::init();
		}
		else if (get_console_type() == con_type_syscon)
		{
			::syscon::init();
		}

		scheduler::once([]()
		{
			console_log = game::Dvar_RegisterString("g_consoleLog", "s2x/logs/console.log", game::DVAR_FLAG_SAVED);
		}, scheduler::main);
	}
}
