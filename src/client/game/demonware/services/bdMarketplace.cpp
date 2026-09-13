#include <std_include.hpp>
#include "../dw_include.hpp"
#include "../hq_marketplace.hpp"
#include "../hq_protocol.hpp"
#include "../hq_vendor.hpp"
#include "../hq_item_data.hpp"
#include "../hq_products.hpp"
#include "steam/steam.hpp"
#include "game/game.hpp"

namespace demonware
{
	namespace
	{
		void add_inventory(service_reply& reply, const hq_economy::item& item)
		{
			auto result = std::make_unique<bdMarketplaceInventory>();
			result->m_playerId = steam::SteamUser()->GetSteamID().bits;
			result->unk = "steam";
			result->m_itemId = item.guid;
			result->m_itemQuantity = item.quantity;
			result->m_itemXp = 0;
			result->m_itemData = item.metadata;
			result->m_expireDateTime = item.expires;
			result->m_expiryDuration = -1;
			result->m_collisionField = item.collision;
			result->m_modDateTime = item.modified;
			reply.add(result);
		}

		// structured: the handler answers with send_struct(), so its failure reply must too.
		template <typename F>
		void guarded(service_server* server, const std::uint8_t task, F callback, const bool structured = false)
		{
			try { callback(); }
			catch (const std::exception& error)
			{
				console::error("[HQ marketplace] task %u failed: %s\n", task, error.what());
				auto reply = server->create_reply(task, BD_HANDLE_TASK_FAILED);
				if (structured) reply.send_struct();
				else reply.send();
			}
			catch (...)
			{
				console::error("[HQ marketplace] task %u failed with an unknown exception\n", task);
				auto reply = server->create_reply(task, BD_HANDLE_TASK_FAILED);
				if (structured) reply.send_struct();
				else reply.send();
			}
		}
	}

	bdMarketplace::bdMarketplace() : service(80, "bdMarketplace")
	{
		this->register_task(42, &bdMarketplace::startExchangeTransaction);
		this->register_task(43, &bdMarketplace::purchaseOnSteamInitialize);
		this->register_task(44, &bdMarketplace::purchaseOnSteamFinalize);
		this->register_task(49, &bdMarketplace::getExpiredInventoryItems);
		this->register_task(58, &bdMarketplace::validateInventoryItemsToken);
		this->register_task(60, &bdMarketplace::steamProcessDurable);
		this->register_task(85, &bdMarketplace::steamProcessDurableV2);
		this->register_task(hq_products::task, &bdMarketplace::getProducts);
		this->register_task(106, &bdMarketplace::purchaseSkus);
		this->register_task(111, &bdMarketplace::getSkusPaginated);
		this->register_task(130, &bdMarketplace::getBalance);
		this->register_task(132, &bdMarketplace::getBalanceV2);
		this->register_task(165, &bdMarketplace::getInventoryPaginated);
		this->register_task(168, &bdMarketplace::putInventoryItemsData);
		this->register_task(193, &bdMarketplace::putPlayersInventoryItems);
		this->register_task(199, &bdMarketplace::pawnItems);
		this->register_task(232, &bdMarketplace::getEntitlements);
		this->register_task(242, &bdMarketplace::unknown242);
	}

	void bdMarketplace::startExchangeTransaction(service_server* server, byte_buffer* buffer) const
	{

		hq_protocol::trace("marketplace_42", buffer->get_remaining());		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdMarketplace::purchaseOnSteamInitialize(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdMarketplace::purchaseOnSteamFinalize(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdMarketplace::getExpiredInventoryItems(service_server* server, byte_buffer* buffer) const
	{
		guarded(server, this->task_id(), [&]
		{
			hq_protocol::trace("expired_request", buffer->get_remaining());
			if (!hq_marketplace::context(buffer) || !hq_protocol::padding(buffer))
			{
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send();
				return;
			}
			const auto data = hq_economy::snapshot();
			auto reply = server->create_reply(this->task_id());
			for (const auto& [key, item] : data.inventory)
				if (item.quantity && item.expires && item.expires <= time(nullptr)) add_inventory(reply, item);
			reply.send();
		});
	}

	void bdMarketplace::validateInventoryItemsToken(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdMarketplace::steamProcessDurable(service_server* server, byte_buffer* buffer) const
	{

		hq_protocol::trace("marketplace_60", buffer->get_remaining());		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdMarketplace::steamProcessDurableV2(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdMarketplace::purchaseSkus(service_server* server, byte_buffer* buffer) const
	{
		if (!game::environment::is_zombies())
		{
			hq_protocol::trace("marketplace_106_rejected", buffer->get_remaining());
			server->create_reply(this->task_id(), BD_HANDLE_TASK_FAILED).send();
			return;
		}
		server->create_reply(this->task_id()).send();
	}

	void bdMarketplace::getBalance(service_server* server, byte_buffer* buffer) const
	{

		if (game::environment::is_zombies())
		{
			hq_protocol::trace("marketplace_130", buffer->get_remaining());
			server->create_reply(this->task_id()).send();
			return;
		}
		// Both SDK balance variants use the same currency reader (A49900).
		getBalanceV2(server, buffer);
	}

	void bdMarketplace::getBalanceV2(service_server* server, byte_buffer* buffer) const
	{

		hq_protocol::trace(this->task_id() == 130 ? "marketplace_130" : "marketplace_132", buffer->get_remaining());		guarded(server, this->task_id(), [&]
		{
			std::uint32_t limit{};
			if (!hq_marketplace::context(buffer) || !buffer->read_uint32(&limit) || !limit || limit > 256 || !hq_protocol::padding(buffer))
			{
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send();
				return;
			}
			const auto data = hq_economy::snapshot();
			auto reply = server->create_reply(this->task_id());
			std::uint32_t count{};
			for (const auto& [id, amount] : data.currencies)
			{
				if (count++ == limit) break;
				auto result = std::make_unique<bdMarketplaceCurrency>();
				result->m_currencyId = id;
				result->m_value = amount;
				if (!game::environment::is_zombies())
				{
					byte_buffer encoded; result->serialize(&encoded);
					hq_protocol::trace(this->task_id() == 130 ? "marketplace_130_currency" : "marketplace_132_currency", encoded.get_buffer());
				}
				reply.add(result);
			}
			reply.send();
		});
	}

	void bdMarketplace::getInventoryPaginated(service_server* server, byte_buffer* buffer) const
	{

		hq_protocol::trace("marketplace_165", buffer->get_remaining());		guarded(server, this->task_id(), [&]
		{
			hq_marketplace::inventory_request request{};
			if (!hq_marketplace::parse_inventory(buffer, request))
			{
				hq_protocol::trace("inventory_invalid", buffer->get_buffer());
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send();
				return;
			}
			const auto data = hq_economy::snapshot();
			auto reply = server->create_reply(this->task_id());
			for (const auto& item : hq_marketplace::inventory_page(data, request, time(nullptr))) add_inventory(reply, item);
			reply.send();
		});
	}

	void bdMarketplace::putPlayersInventoryItems(service_server* server, byte_buffer* buffer) const
	{

		hq_protocol::trace("marketplace_193", buffer->get_remaining());		guarded(server, this->task_id(), [&]
		{
			hq_protocol::trace("put_assumed_request", buffer->get_remaining());
			std::vector<hq_economy::item> items{};
			const auto parsed = hq_marketplace::parse_put(buffer, steam::SteamUser()->GetSteamID().bits, items);
			const auto error = !parsed ? BD_PARAM_PARSE_ERROR : hq_marketplace::put(items) ? BD_NO_ERROR : BD_HANDLE_TASK_FAILED;
			server->create_reply(this->task_id(), error).send();
		});
	}

	void bdMarketplace::pawnItems(service_server* server, byte_buffer* buffer) const
	{

		hq_protocol::trace("marketplace_199", buffer->get_remaining());		guarded(server, this->task_id(), [&]
		{
			hq_protocol::trace("pawn_assumed_request", buffer->get_remaining());
			console::warn("[HQ marketplace] pawnItems uses provisional quantity reconciliation; currency conversion is unavailable\n");
			std::string transaction{};
			std::vector<hq_economy::item> items{};
			const auto parsed = hq_marketplace::parse_pawn(buffer, transaction, items);
			const auto error = !parsed ? BD_PARAM_PARSE_ERROR : hq_marketplace::pawn(transaction, items) ? BD_NO_ERROR : BD_HANDLE_TASK_FAILED;
			server->create_reply(this->task_id(), error).send();
		});
	}

	void bdMarketplace::getEntitlements(service_server* server, byte_buffer* buffer) const
	{

		hq_protocol::trace("marketplace_232", buffer->get_remaining());		guarded(server, this->task_id(), [&]
		{
			hq_protocol::trace("entitlements_assumed_request", buffer->get_remaining());
			if (!hq_marketplace::context(buffer) || !hq_protocol::padding(buffer))
			{
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send();
				return;
			}
			// Inventory ownership is not a Steam entitlement. This local store grants none.
			(void)hq_economy::snapshot();
			server->create_reply(this->task_id()).send();
		});
	}

	void bdMarketplace::getSkusPaginated(service_server* server, byte_buffer* buffer) const
	{
		guarded(server, this->task_id(), [&]
		{
			if (buffer->size() > 65536)
			{
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send();
				return;
			}
			hq_protocol::trace("marketplace_111", buffer->get_remaining());
			hq_marketplace::sku_request request{};
			if (!game::environment::is_zombies() && !hq_marketplace::parse_skus(buffer, request))
			{
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send();
				return;
			}
			auto reply = server->create_reply(this->task_id());
			const auto page = game::environment::is_zombies() ? std::vector<hq_marketplace::sku>{} : hq_marketplace::sku_page(request);
			for (const auto& entry : page)
			{
				auto result = std::make_unique<hq_vendor::catalog_result>();
				result->entry = entry;
				byte_buffer encoded; result->serialize(&encoded);
				hq_protocol::trace_row("marketplace_111_sku", encoded.get_buffer());
				reply.add(result);
			}
			// 27B700 continues on count==limit; short/empty page terminates.
			// Task111 has no NextPageToken result field; do not append JSON/string bytes.
			console::info("[HQ marketplace] SKU page %u limit %u count %u (collection catalog)\n",
				request.page, request.limit, unsigned(page.size()));
			reply.send();
		});
	}

	void bdMarketplace::putInventoryItemsData(service_server* server, byte_buffer* buffer) const
	{
		if (game::environment::is_zombies())
		{
			server->create_reply(this->task_id()).send(); // unchanged generic fallback
			return;
		}
		guarded(server, this->task_id(), [&]
		{
			hq_protocol::trace("marketplace_168", buffer->get_remaining());
			std::string transaction;
			std::vector<hq_item_data::update> updates;
			if (!hq_item_data::parse(buffer, steam::SteamUser()->GetSteamID().bits, transaction, updates))
			{
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send();
				return;
			}
			if (!hq_item_data::apply(transaction, updates))
			{
				server->create_reply(this->task_id(), BD_HANDLE_TASK_FAILED).send();
				return;
			}
			auto result = std::make_unique<hq_item_data::audit_result>();
			result->transaction = transaction;
			byte_buffer encoded; result->serialize(&encoded);
			hq_protocol::trace("marketplace_168_response", encoded.get_buffer());
			auto reply = server->create_reply(this->task_id());
			reply.add(result);
			reply.send();
		});
	}

	// Task 99: the product query the per-frame marketplace pump (0x27D160) issues for
	// every cached SKU that has no product record yet. See hq_products.hpp for the
	// recovered request/result layout; the success callback 0x27B4D0 copies each record
	// into its SKU slot and the pump stops once all 400 slots are filled. (Slice 5 had
	// attributed this task to the reward commit 0x2AF560, which is a client message
	// writer, not a Demonware issuer.)
	void bdMarketplace::getProducts(service_server* server, byte_buffer* buffer) const
	{
		guarded(server, this->task_id(), [&]
		{
			hq_products::request request{};
			if (!hq_products::parse(buffer, request))
			{
				if (!hq_products::rejected++)
				{
					hq_protocol::trace("marketplace_99_invalid", buffer->get_buffer());
					console::warn("[HQ marketplace] task 99: unrecognised request shape\n");
				}
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send();
				return;
			}
			const auto verbose = hq_products::observe(request);
			++hq_products::requests;
			if (verbose)
			{
				hq_protocol::trace("marketplace_99", buffer->get_buffer());
				console::info("[HQ marketplace] task 99 product query page %u count %u limit %u [%s]\n",
					request.page, request.count, request.limit, hq_products::describe(request).c_str());
			}
			auto reply = server->create_reply(this->task_id());
			auto page = hq_products::answer(request);
			for (auto& product : page)
			{
				auto result = std::make_unique<hq_products::product_result>(std::move(product));
				byte_buffer encoded; result->serialize(&encoded);
				hq_protocol::trace_row("marketplace_99_product", encoded.get_buffer());
				reply.add(result);
			}
			hq_products::products += static_cast<std::uint32_t>(page.size());
			reply.send();
		});
	}

	void bdMarketplace::unknown242(service_server* server, byte_buffer* buffer) const
	{
		guarded(server, this->task_id(), [&]
		{
			if (buffer->size() > 65536)
			{
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send_struct();
				return;
			}
			hq_protocol::trace("marketplace_242", buffer->get_remaining());
			if (game::environment::is_zombies())
			{
				server->create_reply(this->task_id()).send_struct();
				return;
			}
			std::string request{};
			if (!buffer->read_struct(&request, 65536) || !hq_protocol::padding(buffer))
			{
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send_struct();
				return;
			}
			++hq_vendor::requests;
			auto result = std::make_unique<hq_vendor::result>();
			if (!hq_vendor::reply_body(request, result->body))
			{
				++hq_vendor::rejected;
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send_struct();
				return;
			}
			byte_buffer encoded;
			result->serialize(&encoded);
			hq_protocol::trace("marketplace_242_response", encoded.get_buffer());
			++hq_vendor::replies;
			auto reply = server->create_reply(this->task_id());
			reply.add(result);
			reply.send_struct();
			hq_vendor::last_reply_ms = hq_vendor::now_ms();
		}, true);
	}

}
