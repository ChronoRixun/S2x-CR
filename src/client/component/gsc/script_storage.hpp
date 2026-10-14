#pragma once

#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace gsc::script_storage
{
	inline constexpr std::size_t max_size = 64 * 1024;

	inline bool valid_name(const std::string_view name)
	{
		if (name.empty() || name.size() > 80 || name.front() == '.' || name.back() == '.') return false;
		for (const auto c : name)
		{
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
		}
		auto stem = std::string{name.substr(0, name.find('.'))};
		for (auto& c : stem) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
		return stem != "CON" && stem != "PRN" && stem != "AUX" && stem != "NUL" &&
			!(stem.size() == 4 && (stem.starts_with("COM") || stem.starts_with("LPT")) &&
				stem[3] >= '0' && stem[3] <= '9');
	}

	inline void reject_reparse(const std::filesystem::path& path)
	{
		const auto attributes = GetFileAttributesW(path.c_str());
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
			throw std::runtime_error("script storage does not follow links");
	}

	inline std::filesystem::path resolve(const std::filesystem::path& root, const std::string_view name)
	{
		if (!valid_name(name)) throw std::runtime_error("script filename must be a plain name (letters, digits, . _ -), at most 80 characters");
		// Check existing ancestors before creating anything, including directory junctions.
		for (auto path = root; !path.empty(); path = path.parent_path())
		{
			reject_reparse(path);
			if (path == path.parent_path()) break;
		}
		std::filesystem::create_directories(root);
		const auto path = root / std::string{name};
		reject_reparse(path);
		return path;
	}

	struct file_handle
	{
		HANDLE value{INVALID_HANDLE_VALUE};
		~file_handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
		file_handle(const file_handle&) = delete;
		file_handle& operator=(const file_handle&) = delete;
		explicit file_handle(const HANDLE handle) : value(handle) {}
	};

	inline std::optional<std::string> read(const std::filesystem::path& root, const std::string_view name)
	{
		const auto path = resolve(root, name);
		file_handle file{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr)};
		if (file.value == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() == ERROR_FILE_NOT_FOUND) return std::nullopt;
			throw std::runtime_error("script file could not be opened");
		}
		BY_HANDLE_FILE_INFORMATION info{};
		if (!GetFileInformationByHandle(file.value, &info) ||
			(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
			info.nFileSizeHigh || info.nFileSizeLow > max_size)
			throw std::runtime_error("script file must be regular text, at most 64 KiB");
		std::string text(info.nFileSizeLow, '\0');
		DWORD count{};
		if (!ReadFile(file.value, text.data(), info.nFileSizeLow, &count, nullptr) || count != info.nFileSizeLow ||
			text.find('\0') != std::string::npos)
			throw std::runtime_error("script file could not be read as text");
		return text;
	}

	inline void write(const std::filesystem::path& root, const std::string_view name, const std::string_view text)
	{
		if (text.size() > max_size || text.find('\0') != std::string_view::npos)
			throw std::runtime_error("script file must be text, at most 64 KiB");
		const auto path = resolve(root, name);
		static std::atomic<unsigned long long> sequence{};
		const auto temporary = root / (".s2x-" + std::to_string(GetCurrentProcessId()) + "-" +
			std::to_string(++sequence) + ".tmp");
		file_handle file{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
		if (file.value == INVALID_HANDLE_VALUE) throw std::runtime_error("script temporary file could not be created");
		DWORD count{};
		const auto saved = WriteFile(file.value, text.data(), static_cast<DWORD>(text.size()), &count, nullptr) &&
			count == text.size() && FlushFileBuffers(file.value);
		CloseHandle(file.value);
		file.value = INVALID_HANDLE_VALUE;
		if (!saved || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			DeleteFileW(temporary.c_str());
			throw std::runtime_error("script file could not be saved");
		}
	}
}
