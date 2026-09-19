#!/usr/bin/env python3
import csv,json,subprocess,sys,tempfile,copy
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
HEADER='sid ordinal rx_ns validated_ns app_ns consumed_ns phase status known bytes checksum'.split()
# Literal canonical IQ16 period, independent of production generation/analyzer.
scalar=[16384,0,15137,6270,11585,11585,6270,15137,0,16384,-6270,15137,-11585,11585,-15137,6270,-16384,0,-15137,-6270,-11585,-11585,-6270,-15137,0,-16384,6270,-15137,11585,-11585,15137,-6270]
digest=14695981039346656037
for n in range(512):
 value=scalar[n%32]&65535
 for byte in [value>>8,value&255]:digest=((digest^byte)*1099511628211)&((1<<64)-1)
rows=[[1,0,1001,1002,1003,2001,1,0,1,1024,digest],[1,256,10000001,10000002,10000003,10000011,1,0,1,1024,digest]]
summary=dict(schema=1,kind='receiver',measurement_start_ns=1000,measurement_end_ns=20001000,duration_seconds=.02,sids=[1],association_generation=1,epoch_seconds=1000,samples_per_packet=256,payload_bytes=1024,checksum_algorithm='fnv1a64',rows=2,generated=2,capture_overflow=0,capture_io_error=False,phase_status_counts=[[0]*4,[2,0,0,0],[0]*4])
summary.update(dict(pool_raw_bytes=0,pool_provider_bytes=0,receiver_object_bytes=0,receiver_auxiliary_bytes=0,capture_bytes=0,writer_stack_bytes=1048576,receiver_stack_bytes=1048576,receiver_loop_state_bytes=0,accounted_bytes=2097152,c_allocation_coverage_available=True))
def run(raw=rows,info=summary,valid=True):
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);(p/'summary.json').write_text(json.dumps(info))
  with (p/'receiver.csv').open('w',newline='') as f:w=csv.writer(f);w.writerow(HEADER);w.writerows(raw)
  process=subprocess.run([sys.executable,str(ROOT/'bench/analyze_receiver.py'),str(p)],capture_output=True,text=True)
  report=json.loads((p/'receiver_analysis.json').read_text());assert process.returncode==(0 if valid else 2),(process.stdout,report.get('invalid_reasons'),report.get('examples'));return report
result=run();assert result['latency']['rx_to_consumed']['p99_ns']==1000;assert result['latency']['app_to_consumed']['p99_ns']==998;assert result['performance_thresholds_applied'] is False;assert result['throughput']['consumption_window']['complex_samples_per_second']==25600
for col,value in [(2,0),(3,999),(4,999),(5,999),(6,0),(8,0),(9,1000),(10,digest^1)]:
 bad=copy.deepcopy(rows);bad[0][col]=value;run(bad,valid=False)
for field,value in [('capture_overflow',1),('capture_io_error',True),('generated',3)]:
 bad=copy.deepcopy(summary);bad[field]=value;run(info=bad,valid=False)
bad=copy.deepcopy(rows);bad[1]=bad[0].copy();run(bad,valid=False)
# Honest pending expiry and overflow have no consumer timestamps or payloadview.
for status in [1,3]:
 raw=[rows[0].copy()];raw[0][4:6]=[0,0];raw[0][7:]=[status,0,0,0]
 info=copy.deepcopy(summary);info['rows']=info['generated']=1;info['phase_status_counts'][1]=[0]*4;info['phase_status_counts'][1][status]=1
 report=run(raw,info);assert report['counts']['measurement_cohort_nondelivered_packets']==1
# Consumer checksum rejection has observed app times but is not delivered.
raw=[rows[0].copy()];raw[0][7]=2;raw[0][10]=0
info=copy.deepcopy(summary);info['rows']=info['generated']=1;info['phase_status_counts'][1]=[0,0,1,0]
run(raw,info)
print('independent receiver analyzer pairing, corruption and nondelivery checks passed')
