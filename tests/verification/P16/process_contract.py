"""Independent public-example process oracle; never constructs protocol packets."""
import argparse, pathlib, re, signal, socket, subprocess, tempfile, time
p=argparse.ArgumentParser();p.add_argument('--mode',choices=['combined','remote','timeout'],required=True);p.add_argument('--combined');p.add_argument('--controller');p.add_argument('--controllee');p.add_argument('--output-dir',required=True);a=p.parse_args();out=pathlib.Path(a.output_dir);out.mkdir(parents=True,exist_ok=True)
def run(command,timeout=10):
 r=subprocess.run(command,capture_output=True,text=True,timeout=timeout);return r

def scan(text,expected,dwell_ns):
 requests=re.findall(r'^request center_hz=(\d+).*handle=(\S+)',text,re.M)
 tuned=re.findall(r'^tuned requested_hz=(\d+) applied_hz=(\d+) AckX=success AckS=matching latency_ns=(\d+) dwell_start_ns=(\d+) dwell_configured_ns=(\d+).*handle=(\S+)',text,re.M)
 dwells=re.findall(r'^dwell complete requested_hz=(\d+) configured_ns=(\d+) elapsed_ns=(\d+)',text,re.M)
 assert [int(x[0]) for x in requests]==expected,(requests,expected)
 assert [int(x[0]) for x in tuned]==expected and len(dwells)==len(expected)
 assert len({x[1] for x in requests})==len(expected)
 for request,tune,dwell in zip(requests,tuned,dwells):
  assert request[1]==tune[5] and tune[0]==tune[1] and int(tune[4])==dwell_ns
  assert int(dwell[0])==int(tune[0]) and int(dwell[1])==dwell_ns and int(dwell[2])>=dwell_ns
 for previous,current in zip(tuned,tuned[1:]):assert int(current[3])>=int(previous[3])+dwell_ns
 summary=re.search(r'summary confirmed=(\d+) received_iq=(\d+) known_iq=(\d+) receiver_drops=(\d+) backend_writes=(\d+) shutdown=(\d+)',text);assert summary
 assert int(summary[1])==len(expected) and int(summary[2])>0 and summary[2]==summary[3]
 return summary

def reserve():
 for base in range(50000,61000,13):
  sockets=[]
  try:
   for port in [base,base+1,base+2,base+6,base+7,base+8]:
    s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));sockets.append(s)
   return base,base+6,sockets
  except OSError:
   for s in sockets:s.close()
 raise AssertionError('no local lane pair')

if a.mode=='combined':
 command=[a.combined,'--deterministic','--dwell-ms','2','--timeout-ms','1000']
 r=run(command);(out/'combined.log').write_text(r.stdout+r.stderr);assert r.returncode==0
 summary=scan(r.stdout,list(range(100000000,100200001,25000)),2000000);assert summary[5]=='9'
 repeated=run([a.combined,'--deterministic','--dwell-ms','1','--sweeps','2']);(out/'repeated.log').write_text(repeated.stdout+repeated.stderr);assert repeated.returncode==0;repeat_summary=scan(repeated.stdout,list(range(100000000,100200001,25000))*2,1000000);assert repeat_summary[5]=='18'
 continuous=run([a.combined,'--deterministic','--dwell-ms','1','--continuous','--duration-ms','10']);(out/'continuous.log').write_text(continuous.stdout+continuous.stderr);assert continuous.returncode==0 and re.search(r'summary confirmed=[1-9]\d* ',continuous.stdout)
 for args in [['--dwell-ms','0'],['--step-hz','0'],['--continuous','--sweeps','1'],['--sweeps','1','--continuous'],['--duration-ms','0']]:
  invalid=run([a.combined,*args]);assert invalid.returncode!=0 and 'request center_hz=' not in invalid.stdout
elif a.mode=='timeout':
 local,peer,sockets=reserve()
 try:
  for s in sockets[:3]:s.close()
  start=time.monotonic();r=run([a.controller,'--local-base-port',str(local),'--peer-base-port',str(peer),'--timeout-ms','50'],4);elapsed=time.monotonic()-start
  (out/'timeout.log').write_text(r.stdout+r.stderr);assert r.returncode==1 and .045<=elapsed<4 and 'scan failed' in r.stderr and 'summary confirmed=0' in r.stdout
  assert 'request center_hz=' not in r.stdout
  sockets[5].setblocking(False)
  try:sockets[5].recv(65535)
  except BlockingIOError:pass
  else:raise AssertionError('timeout emitted cancellation')
 finally:
  for s in sockets:s.close()
else:
 local,peer,sockets=reserve()
 for s in sockets:s.close()
 log=out/'controllee.log'
 with log.open('w') as handle:
  server=subprocess.Popen([a.controllee,'--local-base-port',str(peer),'--peer-base-port',str(local),'--duration-ms','8000'],stdout=handle,stderr=subprocess.STDOUT)
  try:
   deadline=time.monotonic()+5
   while 'ready role=controllee' not in log.read_text():
    assert server.poll() is None and time.monotonic()<deadline
    time.sleep(.01)
   r=run([a.controller,'--local-base-port',str(local),'--peer-base-port',str(peer),'--dwell-ms','3','--timeout-ms','2000'],8);(out/'controller.log').write_text(r.stdout+r.stderr);assert r.returncode==0
   summary=scan(r.stdout,list(range(100000000,100200001,25000)),3000000);assert summary[5]=='0'
   server.send_signal(signal.SIGINT);assert server.wait(timeout=4)==0
   server_text=log.read_text();assert re.search(r'summary .*backend_writes=9 shutdown=',server_text)
  finally:
   if server.poll() is None:server.kill();server.wait(timeout=2)
print('PASS',a.mode)
