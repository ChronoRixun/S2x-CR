#define NOMINMAX
#include "component/gsc/script_storage.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#ifdef NDEBUG
#error Storage regression checks require assertions
#endif

using namespace gsc::script_storage;

template<class F> void rejects(F&& action)
{
	bool rejected{};
	try { action(); } catch (const std::exception&) { rejected = true; }
	assert(rejected);
}

int main()
{
	const auto root = std::filesystem::temp_directory_path() /
		("s2x-script-storage-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
	for (const auto* name : {"", ".", "..", "../escape", "x/y", "x\\y", "C:escape", "C:\\escape", "\\\\server\\file", "file:stream", "file.", "file ", "NUL", "con.txt", "CoM1.txt", "LPT9"})
	{
		assert(!valid_name(name));
		rejects([&] { write(root, name, "bad"); });
	}
	assert(!valid_name(std::string(81, 'a')));
	assert(!read(root, "missing.txt"));
	write(root, "visits-123.txt", "1");
	assert(read(root, "visits-123.txt") == "1");
	write(root, "visits-123.txt", "2\n");
	assert(read(root, "visits-123.txt") == "2\n");
	write(root, "empty.txt", "");
	assert(read(root, "empty.txt") == "");
	write(root, "max.txt", std::string(max_size, 'a'));
	assert(read(root, "max.txt")->size() == max_size);
	for (const auto& bad : {std::string(max_size + 1, 'x'), std::string("a\0b", 3)})
	{
		rejects([&] { write(root, "visits-123.txt", bad); });
		assert(read(root, "visits-123.txt") == "2\n");
	}
	{
		file_handle lock{CreateFileW((root / "visits-123.txt").c_str(), GENERIC_READ, FILE_SHARE_READ,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
		assert(lock.value != INVALID_HANDLE_VALUE);
		rejects([&] { write(root, "visits-123.txt", "3"); });
	}
	assert(read(root, "visits-123.txt") == "2\n");
	write(root, "visits-123.txt", "3");
	assert(read(root, "visits-123.txt") == "3");
	std::filesystem::create_directory(root / "directory");
	// Windows rejects opening a directory without FILE_FLAG_BACKUP_SEMANTICS.
	// Destination directories also cannot be replaced by a text file.
	// Resolve permits the name, while each IO operation rejects its type.
	// These operations must leave the directory intact.
	rejects([&] { read(root, "directory"); });
	rejects([&] { write(root, "directory", "x"); });
	assert(std::filesystem::is_directory(root / "directory"));
	{
		std::ofstream raw{root / "binary.txt", std::ios::binary}; raw.write("x\0y", 3);
	}
	rejects([&] { read(root, "binary.txt"); });
	std::error_code ec;
	std::filesystem::create_symlink(root / "visits-123.txt", root / "link.txt", ec);
	if (!ec)
	{
		rejects([&] { read(root, "link.txt"); });
		rejects([&] { write(root, "link.txt", "bad"); });
		assert(read(root, "visits-123.txt") == "3");
	}
	else std::cout << "SKIP: symbolic link creation unavailable\n";
	for (const auto& entry : std::filesystem::directory_iterator(root))
		assert(!entry.path().filename().string().starts_with(".s2x-"));
	// Only the unique temporary test directory is removed.
	assert(std::filesystem::weakly_canonical(root).parent_path() ==
		std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()));
	assert(root.filename().string().starts_with("s2x-script-storage-"));
	std::filesystem::remove_all(root);
	std::cout << "PASS: filename confinement, text round-trip, limits, atomic replacement and save failure\n";
}
