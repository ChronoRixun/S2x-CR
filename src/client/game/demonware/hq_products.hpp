#pragma once

#include "byte_buffer.hpp"
#include "data_types.hpp"
#include "hq_protocol.hpp"
#include "hq_marketplace.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// bdMarketplace task 99: the product query behind the collection SKU cache.
//
// Evidence (Ghidra S2xFull, image offsets; see build/research/task99-fable.md):
//   * Issuer: the per-frame marketplace pump 0x27D160 (group 5, type 0xA; explicit
//     variant 0x278C30). Once the SKU fetched flag 0x81038A8 is set it walks the
//     400-slot SKU cache (0x81038B0, stride 0x2E8) and collects every slot whose
//     product id (+0x08) is set but whose cached product record (+0x240) is still
//     empty, at most 100 ids per request, then submits the task with the params
//     block 0x816A6E0 (count +0x10B30, ids +0x109A0). It re-issues EVERY FRAME until
//     every slot has its product, which is the 5,715-requests-per-session loop.
//   * SDK prologue 0x9605: `mov r8b,0x63 ; mov dl,0x50` -> bdTaskParams ctor 0xA6B930
//     (service 80, task 99). Request on the wire: context "s2_steam", u32 page (1),
//     u32 count, u32 limit (both = number of ids, <= 100), then one u32 per id.
//   * Result records: 100 x 0x2A8 bytes, vtable 0xB4B298, deserializer 0xA488C0:
//     u32 productId (+0x10); blob (max 135); blob (max 240); blob (max 64) - all three
//     are read into stack temporaries and discarded; u16 (+0x14); u32 (+0x18);
//     u32 itemCount (+0x1C, capacity 10) then per item 0xA486F0: u32 itemId (+0x20),
//     u32 quantity (+0x24); u32 count (+0x1D, <= 4) of u32/u32 pairs; u32 count
//     (+0x40, <= 4) of u32/u32 pairs.
//   * Success callback 0x27B4D0 copies each product (0x20C1A0) into the SKU slot whose
//     product id matches (record at slot+0x10, id at slot+0x240, item count at
//     slot+0x244, item i at slot+0x10+i*0x38: id +0x20, quantity +0x24) and, once all
//     400 slots are filled, raises inventory event 0x17 (success=1) for Lua. The failure
//     callback 0x27B4C0 is empty, so a rejected or empty reply only delays the retry.
//
// Local policy (same as task 111): SKU id = product id = collection item GUID, so every
// collection product carries its GUID at quantity 1. CWL bundles carry five cosmetics.
namespace demonware::hq_products
{
	inline constexpr std::uint8_t task = 99;
	// 0x27D160 stops collecting at 100 ids and the params block holds 100 results.
	inline constexpr std::uint32_t max_ids = 100;
	// Native capacities of the three discarded blobs (0xA488C0).
	inline constexpr std::size_t name_capacity = 135, description_capacity = 240, data_capacity = 64;

	struct request
	{
		std::uint32_t page{}, count{}, limit{};
		std::vector<std::uint32_t> ids{};
	};

	inline bool parse(byte_buffer* buffer, request& result)
	{
		request parsed{};
		std::string context{};
		if (buffer->size() > 65536 || !buffer->read_string(&context) || context != "s2_steam" ||
			!buffer->read_uint32(&parsed.page) || !parsed.page ||
			!buffer->read_uint32(&parsed.count) || !parsed.count || parsed.count > max_ids ||
			!buffer->read_uint32(&parsed.limit) || !parsed.limit || parsed.limit > max_ids) return false;
		for (std::uint32_t i = 0; i < parsed.count; ++i)
		{
			std::uint32_t id{};
			if (!buffer->read_uint32(&id) || !id) return false;
			parsed.ids.push_back(id);
		}
		if (!hq_protocol::padding(buffer)) return false;
		result = std::move(parsed);
		return true;
	}

	// One native product record, serialized in the exact read order of 0xA488C0.
	class product_result final : public bdTaskResult
	{
	public:
		std::uint32_t id{};
		std::vector<std::pair<std::uint32_t, std::uint32_t>> items{}; // itemId, quantity

		void serialize(byte_buffer* buffer) override
		{
			buffer->write_uint32(id); // +0x10 product id
			auto name = std::to_string(id); name.push_back('\0');
			buffer->write_blob(name); // discarded, capacity 135
			buffer->write_blob(std::string(1, '\0')); // discarded, capacity 240
			buffer->write_blob(std::string(1, '\0')); // discarded, capacity 64
			buffer->write_uint16(0); // +0x14
			buffer->write_uint32(0); // +0x18
			const auto count = std::min<std::size_t>(items.size(), 10); // native item capacity
			buffer->write_uint32(static_cast<std::uint32_t>(count)); // +0x1C item count
			for (std::size_t i = 0; i < count; ++i)
			{
				buffer->write_uint32(items[i].first); // item +0x20
				buffer->write_uint32(items[i].second); // item +0x24
			}
			buffer->write_uint32(0); // +0x1D pair count
			buffer->write_uint32(0); // +0x40 pair count
		}
	};

	// Every requested id is answered: the pump only asks for ids it took from our own
	// task 111 catalog, and an unanswered id keeps the per-frame retry alive forever.
	inline std::vector<product_result> answer(const request& value)
	{
		std::vector<product_result> result{};
		const auto offset = (std::uint64_t{value.page} - 1) * value.limit;
		for (auto i = offset; i < value.ids.size() && result.size() < value.limit; ++i)
		{
			product_result product{};
			product.id = value.ids[static_cast<std::size_t>(i)];
			if (const auto entry = hq_marketplace::find_sku(product.id))
				for (const auto id : hq_marketplace::granted_items(*entry)) product.items.emplace_back(id, 1);
			else product.items = {{product.id, 1}};
			result.push_back(std::move(product));
		}
		return result;
	}

	inline std::atomic_uint32_t requests{}, rejected{}, products{};
	inline std::mutex signature_mutex;
	inline std::string signature;

	inline std::string describe(const request& value)
	{
		auto text = std::to_string(value.page) + "," + std::to_string(value.count) + "," + std::to_string(value.limit) + ":";
		for (std::size_t i = 0; i < value.ids.size() && i < 4; ++i) text += (i ? "," : "") + std::to_string(value.ids[i]);
		if (value.ids.size() > 4) text += ",...," + std::to_string(value.ids.back());
		return text;
	}

	// The client used to repeat this task every frame: log the first few requests in
	// full and only a counter afterwards, so a regression can never flood the log again.
	inline bool observe(const request& value)
	{
		constexpr std::uint32_t verbose_requests = 8;
		auto text = describe(value);
		std::lock_guard lock{signature_mutex};
		signature = std::move(text);
		const auto index = requests.load();
		if (index < verbose_requests) return true;
		if (index == verbose_requests || index % 1024 == 0)
			console::warn("[HQ marketplace] task 99: %u product requests so far; the client should stop once every SKU slot has its product (`hqtask99`)\n", index);
		return false;
	}
}
