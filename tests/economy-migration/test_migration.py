"""Synthetic converter tests. No profile paths, game launch, or installation."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('migration', ROOT / 'tools/migrate-cr-economy.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

def source():
    return dict(schemaVersion=1, revision=3, currencies=[dict(currencyID=6,amount=321)],
                inventory=[], achievements=[], transactions=[])

def achievement(name='payroll_officer',kind=5,status='finished'):
    return dict(name=name,challengeName=name,kind=kind,progress=1,progressTarget=1,
        activationTimestamp=100,completionTimestamp=200,offerDay=0,usageTimeTarget=0,usageTime=0,
        status=status,claimTransaction='',successRewards=[])

def item(guid=2,collision=0):
    return dict(guid=guid,quantity=2,collision=collision,modified=42,expires=0,itemData=[0,127,255])

def fixture():
    value=source(); value['inventory']=[item()]
    value['achievements']=[achievement(),achievement('contract_4_headshots_tdm',4,'inProgress')]
    value['transactions']=[dict(id='social:rank:2',request='1234'),dict(id='prestige:mp:1',request='1234'),
                           dict(id='migration:payroll-currency6-v1',request='0')]
    for name in sorted(m.META_NAMES):
        meta=achievement(name,5,'inProgress'); meta['completionTimestamp']=0
        value['achievements'].append(meta)
    return m.migrate(value,12345,contract_policy='refund-token',order_policy='archive-inactive')[0]

class MigrationTests(unittest.TestCase):
    def refused(self,value,**kwargs):
        with self.assertRaises(m.MigrationError): m.migrate(value,12345,**kwargs)
    def test_fixture_deterministic(self):
        self.assertEqual(m.encode(fixture()),m.encode(fixture()))
    def test_wallet_inventory_exact(self):
        value=source(); value['inventory']=[item()]
        out,_=m.migrate(value,12345)
        self.assertEqual(out['currencies'],[{'currencyId':6,'value':321}])
        row=out['inventory'][0]
        self.assertEqual((row['quantity'],row['itemDataHex'],row['collisionField'],row['modDateTime']),(2,'007fff',0,42))
    def test_settled_history(self):
        out=fixture(); state=json.loads(out['achievementState'])
        records={r['name']:r for r in state['achievements']}
        self.assertEqual(records['hit_social_rank_2']['completionCount'],1)
        self.assertEqual(records['cr_progression_rebaseline']['completionCount'],1)
        self.assertEqual(records['payroll_officer_masterprestige']['completionTimestamp'],200)
        receipt=next(r for r in out['processedTransactions'] if r['clientTx']=='prestige:mp:1')
        self.assertEqual(receipt['requestFingerprint'],'migration:cr-settled')
        self.assertTrue(all(len(r['clientTx'])<=64 for r in out['processedTransactions']))
    def test_active_contract_refund(self):
        value=source(); value['achievements']=[achievement('contract_4_headshots_tdm',4,'inProgress')]
        self.refused(value)
        out,report=m.migrate(value,12345,contract_policy='refund-token')
        self.assertEqual(out['inventory'][0]['itemId'],0x5000019)
        self.assertEqual(out['inventory'][0]['quantity'],1)
        self.assertEqual(json.loads(out['achievementState'])['achievements'][0]['status'],'inactive')
        self.assertEqual(len(report['changes']),1)
    def test_refund_collision_and_unknown(self):
        value=source(); value['achievements']=[achievement('contract_4_headshots_tdm',4,'inProgress')]
        value['inventory']=[item(0x5000019)]
        self.refused(value,contract_policy='refund-token')
        value['inventory']=[]; value['achievements'][0]['name']='unknown'
        self.refused(value,contract_policy='refund-token')
    def test_collision_schema(self):
        value=source(); value['inventory']=[item(),item(collision=1)]
        self.refused(value)
        value['inventory'][1]['quantity']=0
        out,_=m.migrate(value,12345)
        self.assertEqual(len(out['inventory']),1)
        self.assertIn('crInventory',out['processedTransactions'][0]['responseJson'])
    def test_bounds_and_types(self):
        for field,value in [('quantity',True),('guid',0),('collision',65536),('expires',-1),('itemData',[0]*65),('modified',1.0)]:
            with self.subTest(field=field):
                src=source(); row=item(); row[field]=value; src['inventory']=[row]; self.refused(src)
        for bad in [True,1.0,-1,2**64]:
            src=source(); src['revision']=bad; self.refused(src)
    def test_duplicates_unknown_and_schema1_target(self):
        for value in [dict(source(),extra=1),fixture()]: self.refused(value)
        value=source(); value['transactions']=[dict(id='a',request=''),dict(id='a',request='')]; self.refused(value)
        with self.assertRaises(m.MigrationError): m.decode(b'{"schemaVersion":1,"schemaVersion":1}')
    def test_claimable_and_orders(self):
        value=source(); value['achievements']=[achievement(status='claimable')]; self.refused(value)
        value['achievements']=[achievement('daily_ch_kills',1,'inProgress')]; self.refused(value)
        out,_=m.migrate(value,12345,order_policy='archive-inactive')
        self.assertEqual(next(r for r in json.loads(out['achievementState'])['achievements'] if r['name']=='daily_ch_kills')['progress'],0)
    def test_currencies(self):
        value=source(); value['currencies'].append(dict(currencyID=7,amount=10)); self.refused(value)
        out,_=m.migrate(value,12345,currency_policy='preserve')
        self.assertEqual(out['currencies'][1],dict(currencyId=7,value=10))
    def test_legacy_and_name_bounds(self):
        legacy=dict(achievements=[dict(name='legacy',kind=5,progress=1,progressTarget=1,fulfilledTimes=2,completionTimestamp=12,status='finished')])
        out,_=m.migrate(source(),12345,legacy=legacy)
        self.assertEqual(next(r for r in json.loads(out['achievementState'])['achievements'] if r['name']=='legacy')['completionCount'],2)
        value=source(); value['achievements']=[achievement('x'*65)]; self.refused(value)
    def test_long_receipts_are_archived(self):
        value=source(); value['transactions']=[dict(id='a'*128,request='q'*1024)]
        out,_=m.migrate(value,12345)
        self.assertEqual(json.loads(out['processedTransactions'][0]['responseJson'])['crReceipt'],value['transactions'][0])
    def test_nonzero_expiry_is_refused(self):
        for expiry in (1, 2**32-2, 2**32-1):
            with self.subTest(expiry=expiry):
                value=source(); value['inventory']=[item()]; value['inventory'][0]['expires']=expiry
                self.refused(value)
        value['inventory'][0]['quantity']=0
        out,_=m.migrate(value,12345)
        self.assertFalse(out['inventory'])
        self.assertIn('crInventory',out['processedTransactions'][0]['responseJson'])
    def test_meta_is_explicit_unpaid_reset(self):
        for name in m.META_NAMES:
            value=source(); row=achievement(name,5,'inProgress'); row['completionTimestamp']=0
            value['achievements']=[row]; self.refused(value)
            out,report=m.migrate(value,12345,order_policy='archive-inactive')
            self.assertNotIn(name,{r['name'] for r in json.loads(out['achievementState'])['achievements']})
            self.assertTrue(any('crAchievement' in r['responseJson'] for r in out['processedTransactions']))
            self.assertIn('Above&Beyond',report['changes'][0])
            for field,settled in [('status','finished'),('status','claimable'),('completionTimestamp',1),('claimTransaction','settled')]:
                changed=dict(row); changed[field]=settled; value['achievements']=[changed]
                self.refused(value,order_policy='archive-inactive')
    def test_legacy_meta_cannot_import_empty_descriptor(self):
        row=dict(name='above_beyond_daily',kind=5,progress=1,progressTarget=6,fulfilledTimes=0,completionTimestamp=0,status='inProgress')
        legacy=dict(achievements=[row])
        self.refused(source(),legacy=legacy)
        out,_=m.migrate(source(),12345,legacy=legacy,order_policy='archive-inactive')
        self.assertNotIn(row['name'],{r['name'] for r in json.loads(out['achievementState'])['achievements']})
        row['fulfilledTimes']=1; self.refused(source(),legacy=legacy,order_policy='archive-inactive')
    def test_archival_and_progress_bounds(self):
        value=source(); value['achievements']=[achievement()]
        value['achievements'][0]['successRewards']=[dict(type='X'*128,id=1,amount=1,achievementName='Y'*128)]*100
        self.refused(value)
        value['achievements'][0]['successRewards']=[]
        value['achievements'][0]['progress']=65536; self.refused(value)
    def test_cli_is_exclusive_and_dry_run(self):
        with tempfile.TemporaryDirectory() as directory:
            src=Path(directory)/'source.json'; dst=Path(directory)/'new.json'; src.write_text(m.encode(source()))
            args=['--input',str(src),'--output',str(dst),'--user-id','12345']
            original=src.read_bytes()
            self.assertEqual(m.main(args),0); self.assertFalse(dst.exists())
            self.assertEqual(m.main(args+['--write']),0); before=dst.read_bytes()
            self.assertEqual(m.main(args+['--write']),2); self.assertEqual(dst.read_bytes(),before)
            self.assertEqual(src.read_bytes(),original)

if __name__=='__main__':
    import sys
    if len(sys.argv)==3 and sys.argv[1]=='--emit-fixture':
        path=Path(sys.argv[2])
        with path.open('x',encoding='utf-8') as output: output.write(m.encode(fixture()))
        assert path.read_text()==m.encode(fixture())
    else:
        unittest.main()
