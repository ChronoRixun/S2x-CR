#!/usr/bin/env python3
"""Offline CR schema-1 conversion; never discovers or modifies a profile."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

U32, U64 = 2**32 - 1, 2**64 - 1
LIMIT = 16 * 1024 * 1024
class MigrationError(ValueError):
    pass

def require(ok, message):
    if not ok:
        raise MigrationError(message)

def integer(value, label, maximum=U64, minimum=0):
    require(type(value) is int and minimum <= value <= maximum, 'invalid integer: ' + label)
    return value

def string(value, label, maximum=128, empty=True):
    require(type(value) is str and len(value.encode('utf-8')) <= maximum and
            (empty or value) and all(32 <= ord(c) <= 126 for c in value), 'invalid text: ' + label)
    return value

def members(value, required, optional=()):
    require(type(value) is dict and set(required) <= set(value) <= set(required) | set(optional),
            'missing or unsupported fields: ' + ','.join(required))

def array(value, label, maximum=10000):
    require(type(value) is list and len(value) <= maximum, 'invalid collection: ' + label)
    return value

def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON member: ' + key)
        result[key] = value
    return result

def encode(value):
    return json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(',', ':'))

def decode(data):
    require(len(data) <= LIMIT, 'input exceeds 16 MiB')
    try:
        return json.loads(data, object_pairs_hook=unique_pairs,
                          parse_constant=lambda s: (_ for _ in ()).throw(MigrationError('nonfinite JSON number')))
    except (ValueError, UnicodeError, RecursionError) as exc:
        raise MigrationError('invalid JSON: ' + str(exc)) from exc

def read(path):
    with Path(path).open('rb') as source:
        data = source.read(LIMIT + 1)
    return decode(data)

def validate_source(source):
    members(source, ('schemaVersion', 'revision', 'currencies', 'inventory', 'achievements', 'transactions'))
    require(integer(source['schemaVersion'], 'schemaVersion') == 1, 'unsupported CR schema')
    integer(source['revision'], 'revision')
    seen = set()
    for row in array(source['currencies'], 'currencies', 256):
        members(row, ('currencyID', 'amount'))
        key = integer(row['currencyID'], 'currencyID', 255)
        integer(row['amount'], 'amount', U32)
        require(key not in seen, 'duplicate currency'); seen.add(key)
    seen = set()
    for row in array(source['inventory'], 'inventory'):
        members(row, ('guid', 'quantity', 'collision', 'modified', 'expires'), ('itemData',))
        key = (integer(row['guid'], 'guid', U32, 1), integer(row['collision'], 'collision', 65535))
        require(key not in seen, 'duplicate inventory collision key'); seen.add(key)
        for name in ('quantity', 'modified', 'expires'):
            integer(row[name], name, U32)
        for byte in array(row.get('itemData', []), 'itemData', 64):
            integer(byte, 'itemData byte', 255)
    seen = set()
    for row in array(source['achievements'], 'achievements'):
        members(row, ('name','challengeName','kind','progress','progressTarget','activationTimestamp',
                      'completionTimestamp','offerDay','usageTimeTarget','usageTime','status','claimTransaction','successRewards'),
                     ('activationGeneration','expiredTimestamp','masterPrestige'))
        name = string(row['name'], 'name', empty=False)
        require(name not in seen, 'duplicate achievement'); seen.add(name)
        for field in ('challengeName','status','claimTransaction'):
            string(row[field], field)
        integer(row['kind'], 'kind', 13, 1)
        for field in ('progress','usageTimeTarget'):
            integer(row[field], field, U32)
        integer(row['progressTarget'], 'progressTarget', U32, 1)
        integer(row['usageTime'], 'usageTime', row['usageTimeTarget'])
        for field in ('activationTimestamp','completionTimestamp','offerDay','activationGeneration','expiredTimestamp'):
            integer(row.get(field, 0), field)
        require(type(row.get('masterPrestige',False)) is bool, 'invalid masterPrestige')
        require(row['status'] in ('available','inactive','inProgress','claimable','finished','expired'), 'invalid achievement status')
        for reward in array(row['successRewards'], 'successRewards', 100):
            members(reward, ('type','id','amount'), ('achievementName',))
            string(reward['type'], 'reward type', empty=False)
            string(reward.get('achievementName',''), 'achievementName')
            integer(reward['id'],'reward id',U32); integer(reward['amount'],'reward amount',U32)
    seen = set()
    for row in array(source['transactions'], 'transactions'):
        members(row, ('id','request'))
        key = string(row['id'], 'receipt id', empty=False)
        string(row['request'], 'receipt request', 1024)
        require(key not in seen, 'duplicate receipt'); seen.add(key)
    return source

# Verified CR retail cost token IDs. No prices or rewards are inferred.
CONTRACT_TOKENS = {
    'contract_zm_ch_kills_1': 0x5000054,
    'contract_zm_ch_kills_2': 0x5000055,
    'contract_zm_ch_kills_3': 0x500005A,
    'contract_zm_ch_jumping_knifes_1': 0x5000057,
    'contract_zm_ch_unique_kills_fr_1': 0x500005B,
    'contract_zm_ch_beheadings_1': 0x500005C,
    'contract_zm_kills_freefire_1': 0x500005E,
    'contract_zm_treasure_1': 0x5000062,
    'contract_4_headshots_tdm': 0x5000019,
    'contract_50_kills_smg': 0x500006c,
    'contract_55_kills_tdm': 0x5000009,
    'contract_ch_lad': 0x50000B9,
    'contract_45_kills': 0x500000c,
    'contract_25_kills_dom': 0x5000010,
    'contract_9_headshots': 0x500001b,
    'contract_25_kills_lmg': 0x5000043,
    'contract_50_kills_lmg': 0x500006d,
    'contract_ch_mas38': 0x5000071,
    'contract_ch_mg81': 0x5000072,
    'contract_ch_sten': 0x5000073,
    'contract_ch_gewehr': 0x5000074,
    'contract_ch_gpmg': 0x5000075,
    'contract_ch_icepick': 0x5000076,
    'contract_ch_trenchknife': 0x5000077,
    'contract_ch_volk': 0x5000078,
    'contract_ch_orso': 0x5000079,
    'contract_ch_combatknife': 0x500007A,
    'contract_ch_enfield': 0x500007B,
    'contract_ch_revolver': 0x500007C,
    'contract_ch_baseballbat': 0x5000083,
    'contract_ch_type5': 0x5000084,
    'contract_ch_sterling': 0x5000085,
    'contract_ch_arisaka': 0x5000086,
    'contract_ch_burstrifle': 0x5000087,
    'contract_ch_autom2': 0x5000088,
    'contract_ch_claymore': 0x5000094,
    'contract_ch_lever_action': 0x5000095,
    'contract_ch_fire_axe': 0x5000096,
    'contract_ch_m1919': 0x5000097,
    'contract_ch_nambu': 0x5000098,
    'contract_ch_blunderbuss': 0x5000099,
    'contract_ch_ptrs': 0x500009A,
    'contract_ch_avs': 0x50000A7,
    'contract_ch_delisle': 0x50000A9,
    'contract_ch_ribeyrolls': 0x50000AA,
    'contract_ch_3line': 0x50000AB,
    'contract_ch_vmg': 0x50000AE,
    'contract_ch_tokyo': 0x50000AF,
    'contract_ch_crossbow': 0x50000B0,
    'contract_ch_emp': 0x50000B1,
    'contract_ch_charlton': 0x50000B2,
    'contract_ch_sledge': 0x50000B4,
    'contract_ch_kg': 0x50000B6,
    'contract_ch_gdb': 0x50000B7,
    'contract_ch_wimmer': 0x50000B8,
    'contract_ch_chatellerault': 0x50000BA,
    'contract_ch_austen': 0x50000BB,
    'contract_ch_bechowiec': 0x50000BC,
    'contract_ch_blyskawica': 0x50000BD,
    'contract_ch_m2': 0x50000BE,
    'contract_ch_erma': 0x50000BF,
    'contract_ch_m36': 0x50000C0,
    'contract_ch_wz': 0x50000C1,
    'contract_ch_dagger': 0x50000C2,
}

META_NAMES = frozenset(('above_beyond_daily', 'above_beyond_weekly',
                        'zm_above_beyond_daily', 'zm_above_beyond_weekly'))

def record(name, kind=5, progress=1, target=1, count=1, timestamp=0, status='finished'):
    return dict(name=name, kind=kind, progress=progress, progressTarget=target,
                completionCount=count, completionTimestamp=timestamp or None, status=status,
                requiresClaim=False, successRewards=[], activationTimestamp=None,
                expirationTimestamp=None, usageTimeTarget=None, usageTimeRemaining=None,
                globalCounterID=None, globalProgressTarget=None)

def migrate(source, user_id, contract_policy='reject', order_policy='reject', currency_policy='require-corrected', legacy=None):
    validate_source(source)
    integer(user_id,'user-id',U64,1)
    require(contract_policy in ('reject','refund-token'), 'invalid contract policy')
    require(order_policy in ('reject','archive-inactive'), 'invalid order policy')
    require(currency_policy in ('require-corrected','preserve'), 'invalid currency policy')
    receipts = {r['id']: r['request'] for r in source['transactions']}
    if currency_policy == 'require-corrected':
        require('migration:payroll-currency6-v1' in receipts or
                not any(r['currencyID'] in (2,7) and r['amount'] for r in source['currencies']),
                'legacy payroll currency ambiguity: explicitly choose --currency-policy preserve or reconcile an input COPY')
    currencies = sorted((dict(currencyId=r['currencyID'], value=r['amount']) for r in source['currencies']), key=lambda r:r['currencyId'])
    inventory = {}
    archived, changes, achievements = [], [], {}; ledger_keys = set()
    def ledger(key, payload, fingerprint='migration:cr-settled'):
        require(len(key.encode()) <= 64 and key not in ledger_keys, 'duplicate/overlong target receipt')
        ledger_keys.add(key); response = encode(payload)
        require(len(response.encode()) <= 6143, 'archival row exceeds native response bound')
        archived.append(dict(clientTx=key, requestFingerprint=fingerprint, responseJson=response, sequence=len(archived)+1))
    for row in sorted(source['transactions'], key=lambda r:r['id']):
        key = row['id'] if row['id'].startswith('prestige:') else 'cr:' + hashlib.sha256(row['id'].encode()).hexdigest()[:60]
        ledger(key, {'crReceipt':row})
    for row in sorted(source['inventory'], key=lambda r:(r['guid'],r['collision'])):
        if not row['quantity']:
            ledger('crzero:' + hashlib.sha256(encode(row).encode()).hexdigest()[:56], {'crInventory':row})
            continue
        require(row['guid'] not in inventory, 'multiple live collision rows cannot fit itemId-keyed target inventory')
        require(row['expires'] == 0, 'nonzero CR expiry has no verified native duration: item ' + str(row['guid']))
        inventory[row['guid']] = dict(playerId=user_id,accountType='steam',itemId=row['guid'],quantity=row['quantity'],
            itemXp=0,itemDataHex=bytes(row.get('itemData',[])).hex(),expireDateTime=row['expires'],
            expiryDuration=0,collisionField=row['collision'],modDateTime=row['modified'])
    for row in sorted(source['achievements'], key=lambda r:r['name']):
        name, kind, status = row['name'], row['kind'], row['status']
        ledger('crstate:' + hashlib.sha256(name.encode()).hexdigest()[:56], {'crAchievement':row})
        require(row['progress'] <= 65535, 'progress exceeds native uint16: ' + name)
        require(len(name.encode()) <= 64, 'achievement name exceeds native 64-byte limit: ' + name)
        if name in META_NAMES:
            require(order_policy == 'archive-inactive', 'Above&Beyond meta requires explicit unpaid reset policy: ' + name)
            require(status not in ('finished', 'claimable') and not row['claimTransaction'] and
                    not row['completionTimestamp'], 'settled/unclaimed Above&Beyond meta requires external reconciliation: ' + name)
            changes.append('archive unpaid Above&Beyond meta; target seeds canonical descriptor: ' + name)
            continue
        # Claimable rewards cannot be priced or settled from the changed target catalog.
        require(status != 'claimable', 'unclaimed valuable reward requires explicit external settlement: ' + name)
        active = status == 'inProgress' and kind != 5
        if active and kind in (4,11):
            require(contract_policy == 'refund-token', 'active contract requires explicit refund policy: ' + name)
            require(name in CONTRACT_TOKENS, 'unknown contract refund token: ' + name)
            token = CONTRACT_TOKENS[name]
            prior = inventory.get(token)
            require(prior is None, 'refund would collide with existing cost token: ' + name)
            inventory[token] = dict(playerId=user_id,accountType='steam',itemId=token,quantity=1,itemXp=0,
                itemDataHex='',expireDateTime=0,expiryDuration=0,collisionField=0,modDateTime=0)
            changes.append('refund permanent token ' + str(token) + ' for ' + name)
            status = 'inactive'
        elif active:
            require(order_policy == 'archive-inactive', 'active order requires explicit archive-inactive policy: ' + name)
            changes.append('archive unpaid free-order progress: ' + name)
            status = 'inactive'
        settled = row['claimTransaction'] != '' or status == 'finished'
        require(not row['claimTransaction'] or status in ('finished','available'), 'ambiguous settled claim state: ' + name)
        achievements[name] = record(name,kind, row['progress'] if not active else 0,row['progressTarget'],
            int(settled),row['completionTimestamp'],'finished' if settled else ('inactive' if status in ('available','expired') else status))
    for key in receipts:
        if key.startswith('social:rank:'):
            tail = key.removeprefix('social:rank:')
            require(tail.isdigit() and str(int(tail)) == tail and 1 <= int(tail) <= 20, 'unsupported social receipt')
            achievements['hit_social_rank_'+tail] = record('hit_social_rank_'+tail)
    payroll = achievements.get('payroll_officer')
    if payroll and payroll['completionTimestamp']:
        for name in ('payroll_officer','payroll_officer_masterprestige'):
            achievements[name] = record(name,count=1,timestamp=payroll['completionTimestamp'],status='inProgress')
    require(not any(key.startswith('payroll:') for key in receipts) or payroll and payroll['completionTimestamp'],
            'payroll receipt has no recoverable cooldown timestamp')
    if legacy is not None:
        members(legacy, ('achievements',))
        for row in array(legacy['achievements'],'legacy achievements'):
            members(row, ('name','kind','progress','progressTarget','fulfilledTimes','completionTimestamp','status'))
            name = string(row['name'],'legacy name',64,False)
            kind = integer(row['kind'],'legacy kind',13,1)
            progress = integer(row['progress'],'legacy progress',65535)
            target = integer(row['progressTarget'],'legacy target',U32,1)
            count = integer(row['fulfilledTimes'],'legacy count',2**31-1)
            timestamp = integer(row['completionTimestamp'],'legacy timestamp')
            require(row['status'] in ('inactive','inProgress','claimable','finished'), 'invalid legacy status')
            require(name not in achievements and name not in {r['name'] for r in source['achievements']}, 'legacy/HQ name collision requires reconciliation: '+name)
            if name in META_NAMES:
                require(order_policy == 'archive-inactive', 'legacy Above&Beyond meta requires explicit unpaid reset policy: ' + name)
                require(row['status'] not in ('finished', 'claimable') and not count and not timestamp,
                        'settled/unclaimed legacy Above&Beyond meta requires external reconciliation: ' + name)
                ledger('crlegacy:' + hashlib.sha256(name.encode()).hexdigest()[:55], {'crLegacyAchievement': row})
                changes.append('archive unpaid legacy Above&Beyond meta; target seeds canonical descriptor: ' + name)
                continue
            require(row['status'] != 'claimable', 'legacy claimable reward unsupported')
            achievements[name] = record(name,kind,progress,target,count,timestamp,row['status'])
    require('cr_progression_rebaseline' not in achievements, 'reserved migration marker collision')
    achievements['cr_progression_rebaseline'] = record('cr_progression_rebaseline')
    state = encode(dict(achievements=sorted(achievements.values(),key=lambda r:r['name']),orderActivations=[]))
    require(len(state.encode()) <= 8*1024*1024, 'achievement state exceeds target bound')
    digest = hashlib.sha256(encode(source).encode()).hexdigest()
    ledger('migration:cr-economy-v1', {'sourceSha256':digest,'revision':source['revision'],'changes':changes}, 'migration:cr-source:'+digest)
    require(len(archived) <= 65536 and len(inventory) <= 65536, 'target capacity exceeded')
    result = dict(schemaVersion=1,currencies=currencies,inventory=sorted(inventory.values(),key=lambda r:r['itemId']),
                  achievementState=state,processedTransactions=archived,nextTransactionSequence=len(archived)+1)
    require(len(encode(result).encode()) <= 64*1024*1024, 'target file exceeds bound')
    return result, dict(sourceSha256=digest,currencies=len(currencies),inventory=len(inventory),
                       achievements=len(achievements),permanentReceipts=len(archived),changes=changes)

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',required=True,help='explicit CR hq_economy.json COPY')
    parser.add_argument('--legacy-achievements',help='optional explicit standalone achievements.json COPY')
    parser.add_argument('--output',required=True,help='new file; existing paths are rejected even in dry-run')
    parser.add_argument('--user-id',required=True,type=int,help='target Steam user ID, no profile discovery')
    parser.add_argument('--contract-policy',choices=('reject','refund-token'),default='reject')
    parser.add_argument('--order-policy',choices=('reject','archive-inactive'),default='reject')
    parser.add_argument('--currency-policy',choices=('require-corrected','preserve'),default='require-corrected')
    parser.add_argument('--write',action='store_true',help='create output exclusively; default prints report only')
    args = parser.parse_args(argv)
    try:
        output = Path(args.output)
        require(not output.exists() and not output.is_symlink(), 'output already exists')
        require(output.resolve() != Path(args.input).resolve(), 'input/output alias')
        result, report = migrate(read(args.input),args.user_id,args.contract_policy,args.order_policy,
                                 args.currency_policy,read(args.legacy_achievements) if args.legacy_achievements else None)
        if args.write:
            data = (encode(result)+'\n').encode()
            with output.open('xb') as destination:
                destination.write(data)
            require(output.read_bytes() == data, 'output verification failed')
        print(json.dumps(dict(report,mode='written' if args.write else 'dry-run',output=str(output)),indent=2))
        return 0
    except (MigrationError,OSError) as exc:
        print('Migration refused: '+str(exc),file=sys.stderr)
        return 2

if __name__ == '__main__':
    sys.exit(main())
