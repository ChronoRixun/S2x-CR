#pragma once

#include "component/console/console.hpp"
#include <atomic>
#include <utility>

namespace demonware::hq_logging
{
	// Pass fixed formats and non-allocating arguments: formatting and output can throw.
	template <typename... Args>
	void safe_warn(const char* format, Args&&... args) noexcept
	{
		try { console::warn(format, std::forward<Args>(args)...); }
		catch (...) {}
	}

	template <typename... Args>
	void safe_info(const char* format, Args&&... args) noexcept
	{
		try { console::info(format, std::forward<Args>(args)...); }
		catch (...) {}
	}

	template <typename... Args>
	void safe_error(const char* format, Args&&... args) noexcept
	{
		try { console::error(format, std::forward<Args>(args)...); }
		catch (...) {}
	}

	// Mark before printing so even a broken logger is attempted only once.
	template <typename... Args>
	void safe_warn_once(std::atomic_bool& warned, const char* format, Args&&... args) noexcept
	{
		if (!warned.exchange(true)) safe_warn(format, std::forward<Args>(args)...);
	}
}
