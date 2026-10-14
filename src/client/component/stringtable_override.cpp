#include <std_include.hpp>
#include <fstream>
#include <new>
#include "loader/component_loader.hpp"

#include "command.hpp"
#include "filesystem.hpp"
#include "scripting.hpp"
#include "console/console.hpp"

#include "game/game.hpp"

#include <utils/hook.hpp>
#include <utils/io.hpp>
#include <utils/memory.hpp>
#include <utils/nt.hpp>
#include <utils/string.hpp>

// Loose-file string table overrides.
//
// A CSV placed at the asset path inside one of the loose file search paths
// (for example %LOCALAPPDATA%\s2x\data\mp\botDivisionTable.csv or
// <game>\s2x\mp\botDivisionTable.csv) replaces the packaged string table
// asset of the same name. Tables that do not exist in any fastfile can be
// added the same way.
namespace stringtable_override
{
	namespace
	{
		constexpr std::size_t max_file_size = 8u * 1024u * 1024u;
		constexpr std::size_t max_rows = 65535;
		constexpr std::size_t max_columns = 1024;
		// Rows x columns after ragged-row padding. The dimension limits alone
		// allow 67 million cells from a 65 KB file of commas and newlines.
		constexpr std::size_t max_table_cells = 1u << 20;

		utils::hook::detour db_find_x_asset_header_hook;

		// Override tables are never freed. The engine may cache StringTable and
		// cell pointers across levels, so replaced tables are leaked instead.
		constexpr char empty_cell[] = "";

		// The engine hashes a cell's string at image offset 0x763AF0 and compares that
		// hash before it compares any string, so a table whose hashes do not match is
		// simply never found. This is that function: h = h * 31 + tolower(c), over
		// sign-extended chars, seeded 0, wrapping in 32 bits.
		int cell_hash(const char* text)
		{
			auto hash = 0u;
			for (; *text; ++text)
			{
				auto c = static_cast<int>(static_cast<signed char>(*text));
				if (c >= 'A' && c <= 'Z')
				{
					c += 'a' - 'A';
				}

				hash = static_cast<unsigned int>(c) + 31u * hash;
			}

			return static_cast<int>(hash);
		}

		struct cache_entry
		{
			game::StringTable* table{};
			std::string real_path{};
			std::filesystem::file_time_type write_time{};
			// The asset name as requested, so invalidation can re-resolve the
			// winning search path rather than only re-stat the old one.
			std::string name{};
		};

		std::mutex cache_mutex;
		std::unordered_map<std::string, cache_entry> cache;

		bool is_safe_asset_name(const std::string_view name)
		{
			return !name.empty()
				&& name.find("..") == std::string_view::npos
				&& name.find(':') == std::string_view::npos
				&& name.front() != '/'
				&& name.front() != '\\';
		}

		struct csv_data
		{
			std::vector<std::vector<std::string>> rows{};
			std::size_t columns{};
		};

		enum class csv_parse_result
		{
			ok,
			too_many_cells,
			unterminated_quote,
		};

		// Rows are separated by '\n' ('\r' is ignored), cells by ','. A cell
		// that starts with a double quote is parsed RFC 4180 style so dumped
		// tables round-trip; anything else is taken verbatim, like the packaged
		// tables. A trailing newline does not create a row; blank lines do, so
		// row indices match editor line numbers. A quoted cell that never closes
		// is rejected rather than silently swallowing the rest of the file.
		csv_parse_result parse_csv(const std::string& text, csv_data& out)
		{
			std::vector<std::string> row{};
			std::string cell{};
			auto at_cell_start = true;

			const auto end_cell = [&]
			{
				row.emplace_back(std::move(cell));
				cell.clear();
				at_cell_start = true;
			};

			const auto end_row = [&]
			{
				end_cell();
				out.columns = std::max(out.columns, row.size());
				out.rows.emplace_back(std::move(row));
				row.clear();
			};

			const auto size = text.size();
			for (std::size_t i = 0; i < size;)
			{
				const auto character = text[i];

				if (at_cell_start && character == '"')
				{
					auto closed = false;
					++i;
					while (i < size)
					{
						if (text[i] == '"')
						{
							if (i + 1 < size && text[i + 1] == '"')
							{
								cell += '"';
								i += 2;
								continue;
							}

							++i;
							closed = true;
							break;
						}

						cell += text[i++];
					}

					if (!closed)
					{
						return csv_parse_result::unterminated_quote;
					}

					at_cell_start = false;
					continue;
				}

				if (character == ',')
				{
					end_cell();
				}
				else if (character == '\n')
				{
					end_row();
				}
				else if (character != '\r')
				{
					cell += character;
					at_cell_start = false;
				}

				++i;

				// The row being built counts too: a line of a million commas must
				// fail at the 1,025th cell, not after building a million strings.
				if (out.rows.size() > max_rows || out.columns > max_columns || row.size() > max_columns)
				{
					return csv_parse_result::too_many_cells;
				}
			}

			// A final row without a trailing newline still counts, including one
			// whose last cell is a quoted empty string (""): the quote consumed
			// the cell start even though no text was produced.
			if (!cell.empty() || !row.empty() || !at_cell_start)
			{
				end_row();
			}

			return out.rows.size() <= max_rows && out.columns <= max_columns
				? csv_parse_result::ok
				: csv_parse_result::too_many_cells;
		}

		enum class build_result
		{
			ok,
			empty,
			too_many_cells,
			allocation_failed
		};

		// The allocations of one construction attempt. They are released together
		// when the attempt fails, so a rejected table leaves nothing behind; once
		// published they belong to the engine-visible table, which keeps pointers
		// into them, and live until the process exits. Blocks come straight from
		// the heap: releasing a few thousand of them must not walk a pool that
		// holds every cell of every table loaded so far. The ledger is sized up
		// front so tracking an allocation cannot itself fail after the block was
		// taken.
		class table_storage
		{
		public:
			table_storage() = default;

			~table_storage()
			{
				release();
			}

			// Sized before any block is taken, so tracking a block cannot fail once
			// the block exists.
			void reserve(const std::size_t capacity)
			{
				owned_.reserve(capacity);
			}

			// Frees every block still owned, most recent first.
			void release()
			{
				for (auto it = owned_.rbegin(); it != owned_.rend(); ++it)
				{
					utils::memory::free(*it);
				}

				owned_.clear();
			}

			table_storage(const table_storage&) = delete;
			table_storage& operator=(const table_storage&) = delete;

			template <typename T>
			T* allocate_array(const std::size_t count)
			{
				return track(utils::memory::allocate_array<T>(count));
			}

			// Nullptr instead of a copy into nothing when the block cannot be had.
			char* duplicate_string(const std::string& string)
			{
				auto* data = utils::memory::allocate_array<char>(string.size() + 1);
				if (data)
				{
					std::memcpy(data, string.data(), string.size());
				}

				return track(data);
			}

			void publish()
			{
				owned_.clear();
			}

		private:
			template <typename T>
			T* track(T* data)
			{
				if (data)
				{
					owned_.push_back(data);
				}

				return data;
			}

			std::vector<void*> owned_{};
		};

		// Builds the engine table for a parsed CSV into `storage`. `out` is set only
		// when every allocation succeeded; the blocks stay owned by `storage` until
		// the caller publishes them, and a failed attempt releases what it took and
		// reports why, so the caller can tell an empty file, an oversized table and
		// memory pressure apart.
		build_result build_table(const std::string& name, const csv_data& csv, table_storage& storage,
			game::StringTable*& out)
		{
			out = nullptr;
			const auto row_count = csv.rows.size();
			const auto column_count = std::max<std::size_t>(csv.columns, 1);
			if (row_count == 0)
			{
				return build_result::empty;
			}

			if (column_count > max_table_cells / row_count)
			{
				return build_result::too_many_cells;
			}

			const auto rows = static_cast<int>(row_count);
			const auto columns = static_cast<int>(column_count);
			const auto cell_count = row_count * column_count;

			// The values, the table, its name and at most one string per cell.
			storage.reserve(cell_count + 3);
			auto* values = storage.allocate_array<game::StringTableCell>(cell_count);
			auto* table = storage.allocate_array<game::StringTable>(1);
			auto* table_name = storage.duplicate_string(name);
			if (!values || !table || !table_name)
			{
				storage.release();
				return build_result::allocation_failed;
			}

			table->name = table_name;
			table->rowCount = rows;
			table->columnCount = columns;
			table->values = values;

			for (auto row = 0; row < rows; ++row)
			{
				const auto& cells = csv.rows[row];
				for (auto column = 0; column < columns; ++column)
				{
					auto& cell = table->values[row * columns + column];
					const auto has_value = column < static_cast<int>(cells.size()) && !cells[column].empty();
					if (has_value)
					{
						cell.string = storage.duplicate_string(cells[column]);
						if (!cell.string)
						{
							storage.release();
							return build_result::allocation_failed;
						}
					}
					else
					{
						cell.string = empty_cell;
					}

					cell.hash = cell_hash(cell.string);
				}
			}

			out = table;
			return build_result::ok;
		}

		// Reads a loose file only after checking its size on the open stream, so
		// an oversized file is rejected before any buffer is allocated.
		bool read_bounded(const std::string& path, std::string& data, bool& oversized)
		{
			oversized = false;
			std::ifstream stream(std::filesystem::path{path}, std::ios::binary);
			if (!stream.is_open())
			{
				return false;
			}

			stream.seekg(0, std::ios::end);
			const auto size = stream.tellg();
			if (size < 0)
			{
				return false;
			}

			if (static_cast<std::uintmax_t>(size) > max_file_size)
			{
				oversized = true;
				return false;
			}

			stream.seekg(0, std::ios::beg);
			data.resize(static_cast<std::size_t>(size));
			stream.read(data.data(), size);
			return static_cast<std::streamsize>(stream.gcount()) == size;
		}

		// Reads and builds the loose table `name` names, if one exists, and says
		// why a file that is present was not used. entry.table stays null whenever
		// the packaged table should be used; entry.real_path names the file so a
		// later change to it is noticed. A built table's blocks are owned by
		// `storage` until the caller publishes them.
		void load_entry(const std::string& name, cache_entry& entry, table_storage& storage)
		{
			if (!filesystem::find_file(name, &entry.real_path))
			{
				return;
			}

			std::string data{};
			auto oversized = false;
			if (!read_bounded(entry.real_path, data, oversized))
			{
				if (oversized)
				{
					console::error("[stringtable] '%s' exceeds %zu bytes; using the packaged table\n",
						entry.real_path.data(), max_file_size);
				}
				else
				{
					console::error("[stringtable] '%s' could not be read; using the packaged table\n",
						entry.real_path.data());
				}

				return;
			}

			// Taken now, while nothing depends on it, so a later failure cannot leave
			// a built table without its timestamp.
			std::error_code error{};
			entry.write_time = std::filesystem::last_write_time(entry.real_path, error);

			csv_data csv{};
			const auto parsed = parse_csv(data, csv);
			if (parsed == csv_parse_result::unterminated_quote)
			{
				console::error("[stringtable] '%s' has an unterminated quoted cell on row %zu; using the packaged table\n",
					entry.real_path.data(), csv.rows.size() + 1);
				return;
			}

			if (parsed != csv_parse_result::ok)
			{
				console::error("[stringtable] '%s' has too many rows or columns; using the packaged table\n",
					entry.real_path.data());
				return;
			}

			switch (build_table(name, csv, storage, entry.table))
			{
			case build_result::ok:
				// Best effort: a message that cannot be formatted must not cost the
				// table it announces.
				try
				{
					console::info("[stringtable] loaded '%s' from '%s' (%d rows x %d columns)\n",
						name.data(), entry.real_path.data(), entry.table->rowCount, entry.table->columnCount);
				}
				catch (const std::bad_alloc&)
				{
				}

				break;
			case build_result::empty:
				console::error("[stringtable] '%s' is empty; using the packaged table\n", entry.real_path.data());
				break;
			case build_result::too_many_cells:
				console::error("[stringtable] '%s' pads to more than %zu cells; using the packaged table\n",
					entry.real_path.data(), max_table_cells);
				break;
			case build_result::allocation_failed:
				console::error("[stringtable] '%s' could not be loaded: out of memory; using the packaged table\n",
					entry.real_path.data());
				break;
			}
		}

		// The one place a lookup can run out of memory. Everything that allocates on
		// behalf of the attempt - the owning copies of the name, the read, the
		// parse, the table and the cache entry - happens inside the try, and a
		// failure anywhere destroys the attempt and answers with the packaged table.
		// A table's blocks are published only once the cache holds it, so nothing
		// is handed to the engine that this code does not also remember.
		game::StringTable* find_override(const char* name)
		{
			if (!name || !is_safe_asset_name(name))
			{
				return nullptr;
			}

			try
			{
				const std::string asset(name);
				const auto key = utils::string::to_lower(asset);

				{
					std::lock_guard _(cache_mutex);
					const auto cached = cache.find(key);
					if (cached != cache.end())
					{
						return cached->second.table;
					}
				}

				// Disk access happens without the cache lock held.
				cache_entry entry{};
				entry.name = asset;
				table_storage storage{};
				load_entry(asset, entry, storage);

				auto* table = entry.table;
				std::lock_guard _(cache_mutex);
				const auto [cached, inserted] = cache.try_emplace(key, std::move(entry));
				if (!inserted)
				{
					// Another lookup got there first: its table is the one in use, and
					// this attempt's blocks go back with `storage`.
					return cached->second.table;
				}

				if (table)
				{
					storage.publish();
				}

				return table;
			}
			catch (const std::bad_alloc&)
			{
				// Best effort: the message itself may not be affordable either.
				try
				{
					console::error("[stringtable] '%s' could not be loaded: out of memory; using the packaged table\n", name);
				}
				catch (const std::bad_alloc&)
				{
				}

				return nullptr;
			}
		}

		game::StringTable* find_override(const std::string& name)
		{
			return find_override(name.data());
		}

		game::XAssetHeader db_find_x_asset_header_stub(const game::XAssetType type, const char* name,
			const int allow_create_default)
		{
			if (type != game::ASSET_TYPE_STRINGTABLE || !name || !*name)
			{
				return db_find_x_asset_header_hook.invoke<game::XAssetHeader>(type, name, allow_create_default);
			}

			if (auto* table = find_override(name))
			{
				game::XAssetHeader header{};
				header.stringTable = table;
				return header;
			}

			return db_find_x_asset_header_hook.invoke<game::XAssetHeader>(type, name, allow_create_default);
		}

		// Called on level shutdown so edited or newly added files are picked up
		// by the next map load without restarting the game.
		void invalidate_changed_entries()
		{
			std::lock_guard _(cache_mutex);

			for (auto entry = cache.begin(); entry != cache.end();)
			{
				auto drop = entry->second.table == nullptr;

				if (!drop)
				{
					// A copy that appeared in a higher-priority search path (AppData
					// before the game folder) must win on the next map even though the
					// file that was loaded is unchanged.
					std::string current_path{};
					drop = !filesystem::find_file(entry->second.name, &current_path) ||
						current_path != entry->second.real_path;
				}

				if (!drop)
				{
					std::error_code error{};
					const auto write_time = std::filesystem::last_write_time(entry->second.real_path, error);
					drop = error || write_time != entry->second.write_time;
				}

				entry = drop ? cache.erase(entry) : std::next(entry);
			}
		}

		void clear_cache()
		{
			std::lock_guard _(cache_mutex);
			cache.clear();
		}

		std::string escape_csv_cell(const char* value)
		{
			std::string text = value ? value : "";
			if (text.find_first_of(",\"\r\n") == std::string::npos)
			{
				return text;
			}

			return "\"" + utils::string::replace(text, "\"", "\"\"") + "\"";
		}

		void dump_string_table(const command::params& params)
		{
			if (params.size() < 2)
			{
				console::info("Usage: dumpstringtable <asset name> [output file]\n");
				console::info("Writes a loaded string table as CSV to s2x/dump/<asset name> (or s2x/<output file>).\n");
				console::info("Example: dumpstringtable mp/botDivisionTable.csv\n");
				return;
			}

			const std::string name = params[1];
			if (!is_safe_asset_name(name))
			{
				console::error("dumpstringtable: invalid asset name '%s'\n", name.data());
				return;
			}

			auto* table = find_override(name);
			if (!table)
			{
				if (!game::DB_XAssetExists(game::ASSET_TYPE_STRINGTABLE, name.data()))
				{
					console::error("dumpstringtable: string table '%s' is not loaded (use listassetpool %d <filter> to list names)\n",
						name.data(), static_cast<int>(game::ASSET_TYPE_STRINGTABLE));
					return;
				}

				table = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, name.data(), 0).stringTable;
			}

			if (!table || !table->values)
			{
				console::error("dumpstringtable: string table '%s' has no data\n", name.data());
				return;
			}

			std::string output{};
			for (auto row = 0; row < table->rowCount; ++row)
			{
				for (auto column = 0; column < table->columnCount; ++column)
				{
					if (column)
					{
						output += ',';
					}

					output += escape_csv_cell(table->values[row * table->columnCount + column].string);
				}

				output += "\r\n";
			}

			const auto base = utils::nt::library{}.get_folder() / "s2x";
			const auto path = params.size() >= 3
				? base / params[2]
				: base / "dump" / name;

			if (!utils::io::write_file(path.generic_string(), output))
			{
				console::error("dumpstringtable: failed to write '%s'\n", path.generic_string().data());
				return;
			}

			console::info("dumpstringtable: wrote '%s' (%d rows x %d columns) to '%s'\n",
				name.data(), table->rowCount, table->columnCount, path.generic_string().data());
		}
	}

	class component final : public generic_component
	{
	public:
		void post_unpack() override
		{
			db_find_x_asset_header_hook.create(game::DB_FindXAssetHeader, db_find_x_asset_header_stub);

			scripting::on_shutdown([](int)
			{
				invalidate_changed_entries();
			});

			command::add("dumpstringtable", dump_string_table);

			command::add("reloadstringtables", []
			{
				clear_cache();
				console::info("[stringtable] cache cleared; loose tables are reloaded on their next lookup\n");
			});
		}
	};
}

REGISTER_COMPONENT(stringtable_override::component)
