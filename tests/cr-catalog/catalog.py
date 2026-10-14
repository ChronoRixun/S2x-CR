"""Check embedded offers/publisher policy without reading any player profile."""
import json
import pathlib
import re
import struct
root = pathlib.Path(__file__).resolve().parents[2]
resource = root / "src/client/resources/dw"
policy = (root / "src/client/game/demonware/marketplace/cr_catalog.hpp").read_text()
products = re.findall(r'\{(0x[0-9A-Fa-f]+|\d+), (\d+), "([^"]+)", "([^"]+)", \{([^}]+)\}, (true|false)(?:, (true|false))?\}', policy)
assert len(products) == 75
j = json.loads((resource / "achievement_offers.json").read_text())
offers = j["offers"]
contracts = []
bundles = {}
for id, price, data, text, items, bundle, consumable in products:
    ids = [int(x.strip(), 0) for x in items.split(",")]
    assert len(data) < 64 and len(text) < 135
    assert 0 < len(ids) <= 10 and len(set(ids)) == len(ids)
    if bundle == "true":
        tag = re.search(r"t:([^;]+)", data)[1]
        bundles[tag] = ids
        assert f"l:{id}|1" in data
    elif consumable == "true":
        assert int(price) == 1000 and ids == [int(id,0)] and int(id,0) in (2,6)
    else:
        assert len(ids) == 1
        matching = [o for o in offers.values() if o.get("costItemID") == ids[0]]
        assert len(matching) == 1
        o = matching[0]
        assert o["kind"] in (4,11) and o["progressTarget"] > 0 and o["usageTimeTarget"] > 0
        if int(price) == 2500:
            reward = o["successRewards"][0]["product"]["items"][0]
            assert reward["quantity"] == 1 and reward["id"] & 0xFFF == 0x100
            contracts.append(ids[0])
assert len(contracts) == 48 and len(bundles) == 9
assert bundles["S2X_CODE"] == [0x2000004, 0x800093, 0x2400225, 0x6600100]
assert 0x80013A in bundles["S2X_VIPER"] and 0x80013A in bundles["S2X_ENDOWMENT"]
vars = json.loads((resource / "publisher_variables/mp_tu26_1_20.json").read_text())
refs = {k: v for k,v in vars.items() if k.startswith("online_quartermaster_special_bundle_") and k.endswith("_ref")}
assert set(refs.values()) == set(bundles)
for k,tag in refs.items():
    assert vars[k[:-4] + "_start"] == "0" and vars[k[:-4] + "_end"] == "2147483647"
for name in ("daily_ch_dom_wins", "daily_ch_tdm_wins"):
    assert offers[name]["progressTarget"] == 1
    assert offers[name]["successRewards"] == [{"type":"grant_currency", "currency":{"id":7,"amount":250}}]
used = set()
for rotation in j["rotations"]:
    kind = rotation["kind"]
    count = 9 if kind in (4,11) else 1 if kind in (3,10) else 3 if kind in (2,9) else 6
    assert 0 < len(rotation["sets"]) <= 256
    for group in rotation["sets"]:
        assert len(group) == count and len(set(group)) == count
        for name in group:
            assert offers[name]["kind"] == kind
            used.add(name)
    used.update(rotation.get("alwaysAvailable", []))
assert used == set(offers)
# Local SKU/product identities must not alias captured retail records.
custom = {int(p[0],0) for p in products}
for filename in ("marketplace_skus.bin", "marketplace_products.bin"):
    data = (resource / filename).read_bytes()
    version,count = struct.unpack_from("<II",data,8)
    pos = 16
    captured = set()
    for _ in range(count):
        id,size = struct.unpack_from("<II",data,pos)
        captured.add(id)
        pos += 8 + size
    assert pos == len(data) and custom.isdisjoint(captured)
print("PASS: 48 priced Rare weapon contracts,64total contracts,9bundle contents/tile schedules,2ACrare products, SocialScore win rewards, rotation quotas, no captured identity collisions")
