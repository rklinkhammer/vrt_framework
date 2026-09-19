#!/usr/bin/env python3
"""Independent Fraction/table oracle; no production decoder builds expected values."""
from fractions import Fraction as F
import bisect,subprocess,sys,json

def numeric(spec,raw):
 k,n,f,e=spec
 signed=k%2
 if k>=4:
  m=n-e;mantissa=raw>>e;exponent=raw&((1<<e)-1)
  if signed and mantissa&(1<<(m-1)):mantissa-=1<<m
  return F(mantissa)*F(2)**(exponent-((1<<e)-1)-m+signed)
 integer=raw
 if signed and raw&(1<<(n-1)):integer-=1<<n
 return F(integer)*F(2)**(-f if k>=2 else -n+signed)

def canonical_binary(v):
 if not v:return (0,0,0)
 neg=int(v<0);v=abs(v);m=v.numerator;e=-(v.denominator.bit_length()-1)
 while not m&1:m//=2;e+=1
 return m,e,neg

queries=[];expected=[];labels=[]
def add(q,r,label):queries.append(' '.join(map(str,q)));expected.append(r);labels.append(label)
specs=[]
for n in range(1,7):
 specs.extend([(0,n,0,0),(1,n,0,0)])
 for f in range(n):specs.extend([(2,n,f,0),(3,n,f,0)])
for n in range(2,9):
 for e in range(1,min(4,n-1)+1):specs.extend([(4,n,0,e),(5,n,0,e)])
for s in specs:
 k,n,f,e=s;table={}
 for raw in range(1<<n):
  v=numeric(s,raw);add((0,*s,raw),'D '+' '.join(map(str,canonical_binary(v))),('decode',s,raw))
  if v not in table or (k>=4 and (raw&((1<<e)-1))<(table[v]&((1<<e)-1))):table[v]=raw
 table[F(0)]=0
 values=sorted(table);lo,hi=values[0],values[-1];step=min(b-a for a,b in zip(values,values[1:]))
 candidates={lo-step,hi+step,lo,hi,F(0)}
 # Every adjacent interval, with quarter, exact-half, and three-quarter inputs.
 for a,b in zip(values,values[1:]):
  candidates.update((a,(3*a+b)/4,(a+b)/2,(a+3*b)/4,b))
 for v in sorted(candidates):
  mag,exp,neg=canonical_binary(v)
  for precision in (0,1):
   for rounding in range(4):
    for overflow in (0,1):
     result='X'
     if v<lo or v>hi:
      if precision and overflow:result=f'E {table[lo if v<lo else hi]} 0 1'
     elif v in table:result=f'E {table[v]} 0 0'
     elif precision:
      at=bisect.bisect_left(values,v);a,b=values[at-1],values[at]
      if rounding==1:chosen=a if v>0 else b
      elif rounding==2:chosen=a
      elif rounding==3:chosen=b
      elif v-a<b-v:chosen=a
      elif v-a>b-v:chosen=b
      else:chosen=a if (a/(b-a)).denominator==1 and (a/(b-a)).numerator%2==0 else b
      result=f'E {table[chosen]} 1 0'
     add((1,*s,mag,exp,neg,precision,rounding,overflow),result,('encode',s,str(v),precision,rounding,overflow))
# Full64-bit anchors and extreme exponent classification require no float host behavior.
for s in [(0,64,0,0),(1,64,0,0),(2,64,15,0),(3,64,15,0)]+[(k,64,0,e) for k in (4,5) for e in range(1,7)]:
 for raw in (0,1,1<<63,(1<<64)-1,(1<<64)-2):add((0,*s,raw),'D '+' '.join(map(str,canonical_binary(numeric(s,raw)))),('wide',s,raw))
for exp in (-32768,32767):
 for neg in (0,1):
  for rounding in range(4):
   s=(1,8,0,0);result='X' if exp>0 else f'E {255 if neg and rounding==2 else 1 if not neg and rounding==3 else 0} 1 0'
   add((1,*s,1,exp,neg,1,rounding,0),result,('extreme',exp,neg,rounding))
p=subprocess.run([sys.argv[1]],input='\n'.join(queries)+'\n',text=True,capture_output=True)
assert p.returncode==0,p.stderr
actual=p.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected))
for index,(a,e) in enumerate(zip(actual,expected)):
 if a!=e:raise AssertionError((index,labels[index],queries[index],a,e))
print(json.dumps({'status':'PASS','specifications':len(specs),'cases':len(queries),'decode_cases':sum(q.startswith('0 ') for q in queries),'encode_cases':sum(q.startswith('1 ') for q in queries)}))
