import csv,struct,sys,collections
S,R=sys.argv[1],sys.argv[2]
U={'PSX.EXE':'SLUS_001.58','GAME.EXE':'GAME.EXE','OPEN.EXE':'OPEN.EXE','END.EXE':'END.EXE'}
def load(p):
    b=open(p,'rb').read(); return struct.unpack_from('<I',b,0x18)[0],b[0x800:]
def tsv(p):
    hdr=None
    for l in open(p):
        if l.startswith('#'): continue
        r=l.rstrip('\n').split('\t')
        if hdr is None: hdr=r; continue
        yield dict(zip(hdr,r))
fmap=[dict(image=r['image'],jp_va=r['jp_va'],size=r['jp_size'],delta=hex(int(r['us_va'],16)-int(r['jp_va'],16))) for r in csv.DictReader(open(f'{S}/fmap_final.tsv'),delimiter='\t') if r['status']=='exact' and r['jp_va']]
def site_us(img,va):
    for r in fmap:
        if r['image']==img:
            a=int(r['jp_va'],16)
            if a<=va<a+int(r['size'],16): return va+int(r['delta'],16)
    return None
# speed: index per image
idx=collections.defaultdict(list)
for r in fmap: idx[r['image']].append((int(r['jp_va'],16),int(r['size'],16),int(r['delta'],16)))
import bisect
for k in idx: idx[k].sort()
starts={k:[x[0] for x in v] for k,v in idx.items()}
def site_us(img,va):
    L=idx[img]; i=bisect.bisect_right(starts[img],va)-1
    if i>=0 and L[i][0]<=va<L[i][0]+L[i][1]: return va+L[i][2]
pairs=collections.defaultdict(collections.Counter)
imgs={k:load(f'{S}/us/{v}') for k,v in U.items()}
def w(img,va):
    t,b=imgs[img]; return struct.unpack_from('<I',b,va-t)[0]
bad=0
for r in tsv(f'{S}/jpcfg/relocs.tsv'):
    img=r['image']; k=r['kind']
    if k=='mips_hi16_lo16':
        s=site_us(img,int(r['site_va'],16)); p=site_us(img,int(r['paired_site_va'],16))
        if s is None or p is None: continue
        hi=w(img,s)&0xffff; lo=w(img,p)&0xffff
        if lo&0x8000: lo-=0x10000
        ut=((hi<<16)+lo)&0xffffffff
        pairs[(img,r['target_region'])][(int(r['target_va'],16),ut)]+=1
for (img,reg),c in sorted(pairs.items()):
    by=collections.defaultdict(collections.Counter)
    for (j,u),n in c.items(): by[j][u-j]+=n
    conflicts=[j for j,d in by.items() if len(d)>1]
    segs=[];prev=None
    for j in sorted(by):
        d=by[j].most_common(1)[0][0]
        if prev is None or d!=prev: segs.append([j,j,d,1]); prev=d
        else: segs[-1][1]=j; segs[-1][3]+=1
    print(f'== {img} {reg}: {len(by)} targets, {len(conflicts)} with conflicting deltas, {len(segs)} segments')
    for a,b,d,n in segs: print(f'   {a:#x}..{b:#x} delta {d:+#x} ({n} targets)')
import pickle; pickle.dump(dict(pairs),open(f'{S}/datapairs.pkl','wb'))
