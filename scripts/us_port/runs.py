# Group JP functions without a byte-identical US copy into runs between mapped neighbours,
# then split the US gap into functions by jr-ra boundaries and compare counts.
import csv,struct,sys
S=sys.argv[1]
rows=list(csv.DictReader(open(f'{S}/jp_us_map.tsv'),delimiter='\t'))
U={'PSX.EXE':'SLUS_001.58','GAME.EXE':'GAME.EXE','OPEN.EXE':'OPEN.EXE','END.EXE':'END.EXE'}
def load(p):
    b=open(p,'rb').read(); return struct.unpack_from('<I',b,0x18)[0],b[0x800:]
def ends(b,t,a,e):
    # split [a,e) after each "jr ra; <delay>" pair
    out=[];s=a;o=a
    while o<e:
        w=struct.unpack_from('<I',b,o-t)[0]
        if w==0x03e00008:
            n=o+8
            while n<e and struct.unpack_from('<I',b,n-t)[0]==0: n+=4  # padding
            out.append((s,o+8-s)); s=n; o=n; continue
        o+=4
    if s<e: out.append((s,e-s))
    return out
runs=[]
for img in U:
    R=[r for r in rows if r['image']==img]
    i=0
    while i<len(R):
        if R[i]['us_va']: i+=1; continue
        j=i
        while j<len(R) and not R[j]['us_va']: j+=1
        prev=R[i-1] if i else None; nxt=R[j] if j<len(R) else None
        runs.append((img,R[i:j],prev,nxt)); i=j
for img,run,prev,nxt in runs:
    t,b=load(f'{S}/us/{U[img]}')
    a=int(prev['us_va'],16)+int(prev['size'],16) if prev else t
    # JP gap between prev end and first run function (padding) carried over
    ja=int(prev['jp_va'],16)+int(prev['size'],16) if prev else int(run[0]['jp_va'],16)
    a+=int(run[0]['jp_va'],16)-ja
    e=int(nxt['us_va'],16)-(int(nxt['jp_va'],16)-(int(run[-1]['jp_va'],16)+int(run[-1]['size'],16))) if nxt else a
    us=ends(b,t,a,e)
    tag='OK ' if len(us)==len(run) else 'MISMATCH'
    print(f"{tag} {img} {len(run)} jp fn(s) [{run[0]['name'] or run[0]['jp_va']} .. {run[-1]['name'] or run[-1]['jp_va']}]  us {a:#x}-{e:#x}: {len(us)} pieces")
    if tag!='OK ':
        for r in run: print('     jp',r['jp_va'],r['size'],r['name'])
        for s,n in us: print('     us',hex(s),hex(n))
