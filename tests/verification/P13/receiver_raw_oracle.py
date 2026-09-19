#!/usr/bin/env python3
"""Independent receiver CSV audit; no production analyzer imports."""
import csv,json,math,sys
from pathlib import Path
from collections import Counter,defaultdict

def inspect(directory):
 p=Path(directory);summary=json.loads((p/'summary.json').read_text());errors=[]
 with (p/'receiver.csv').open() as file:rows=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(file)]
 if len(rows)!=summary['rows'] or len(rows)!=summary['generated']:errors.append('row counts')
 for name in ['capture_overflow','capture_io_error','startup_timeout']:
  if summary[name]:errors.append(name)
 # Literal IQ16 scalars, repeated16times to256complex pairs.
 values=[16384,0,15137,6270,11585,11585,6270,15137,0,16384,-6270,15137,-11585,11585,-15137,6270,-16384,0,-15137,-6270,-11585,-11585,-6270,-15137,0,-16384,6270,-15137,11585,-11585,15137,-6270]
 expected=14695981039346656037
 for n in range(512):
  v=values[n%32]&65535
  for b in [v>>8,v&255]:expected=((expected^b)*1099511628211)&((1<<64)-1)
 stages=defaultdict(list);phase_counts=[[0]*4 for _ in range(3)];keys=set();stream=defaultdict(Counter);consume_packets=0;measure_packets=0
 start,end=summary['measurement_start_ns'],summary['measurement_end_ns']
 for r in rows:
  key=(r['sid'],r['ordinal'],r['rx_ns'])
  if key in keys:errors.append('duplicate key')
  keys.add(key);phase=0 if r['rx_ns']<start else 1 if r['rx_ns']<end else 2
  if r['phase']!=phase:errors.append('phase')
  phase_counts[phase][r['status']]+=1
  if r['validated_ns']<r['rx_ns']:errors.append('validation ordering')
  if r['status']==0:
   stream[r['sid']]['delivered']+=1;stream[r['sid']]['known']+=r['known']
   if not r['known'] or r['bytes']!=1024 or r['ordinal']%16 or r['checksum']!=expected:errors.append('payload/metadata')
   if not r['validated_ns']<=r['app_ns']<=r['consumed_ns']:errors.append('consumer ordering')
   if phase==1:
    measure_packets+=1;stream[r['sid']]['measured_packets']+=1
    for label,a,b in [('rx_to_validated','rx_ns','validated_ns'),('validated_to_app','validated_ns','app_ns'),('app_to_consumed','app_ns','consumed_ns'),('rx_to_consumed','rx_ns','consumed_ns')]:stages[label].append(r[b]-r[a])
   consume_packets+=start<=r['consumed_ns']<end
  elif r['status'] in [1,3]:
   if r['app_ns'] or r['consumed_ns'] or r['known'] or r['checksum']:errors.append('fabricated nondelivery')
  elif r['status']==2 and r['app_ns'] and not r['validated_ns']<=r['app_ns']<=r['consumed_ns']:errors.append('reject ordering')
 if phase_counts!=summary['phase_status_counts']:errors.append('phase matrix')
 for s in summary['streams']:
  for key in ['delivered','known','measured_packets']:
   if stream[s['sid']][key]!=s[key]:errors.append('stream '+key)
  if s['measured_samples']!=256*stream[s['sid']]['measured_packets']:errors.append('stream samples')
 def stats(v):
  v=sorted(v);return {'count':len(v),'p50_ns':v[math.ceil(.5*len(v))-1] if v else None,'p99_ns':v[math.ceil(.99*len(v))-1] if v else None,'max_ns':max(v) if v else None}
 duration=(end-start)/1e9
 return {'errors':errors,'rows':len(rows),'phase_status_counts':phase_counts,'latency':{k:stats(v) for k,v in stages.items()},'measurement_ingress_samples_per_second':measure_packets*256/duration,'consumption_window_samples_per_second':consume_packets*256/duration,'critical_cpp_allocations':summary['critical_cpp_allocations'],'critical_c_allocations':summary['critical_c_allocations'],'accounted_bytes':summary['accounted_bytes']}
if __name__=='__main__':
 result=inspect(sys.argv[1]);print(json.dumps(result,indent=2));raise SystemExit(bool(result['errors']))
