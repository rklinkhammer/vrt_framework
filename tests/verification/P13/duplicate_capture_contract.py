import pathlib,json,csv,tempfile,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[3];src=pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as name:
 p=pathlib.Path(name)
 for mode in ['identical','conflicting']:
  for f in ['summary.json','trace.csv','peer.csv']:(p/f).write_bytes((src/f).read_bytes())
  s=json.loads((p/'summary.json').read_text());s['peer_capture_policy']='stateless_checked_raw_v1'
  with (p/'peer.csv').open() as f:r=csv.DictReader(f);fields=r.fieldnames;rows=list(r)
  extra=next(x.copy() for x in rows if x['kind']=='3');extra['monotonic_ns']=str(int(extra['monotonic_ns'])+1)
  if mode=='conflicting':extra['ack_cam']=str(int(extra['ack_cam'])^(1<<10));extra['success']='0';s['ack_failed']+=1
  rows.append(extra);s['ack_x']+=1;s['peer_rows']+=1;s['peer_generated']+=1
  with (p/'peer.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
  (p/'summary.json').write_text(json.dumps(s));r=subprocess.run(['python3',str(root/'bench/analyze.py'),str(p),'--minimum-duration-seconds','0'],capture_output=True,text=True)
  print(mode,r.returncode,r.stdout.strip());assert r.returncode==(1 if mode=='identical' else 2)
