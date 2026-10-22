"""Build one JP->US address map over every curated function and data item."""
import csv,struct,sys,collections,bisect,pickle
S,R=sys.argv[1],sys.argv[2]
J={'PSX.EXE':'PSX.EXE','GAME.EXE':'GAME.EXE','OPEN.EXE':'OPEN.EXE','END.EXE':'END.EXE'}
U={'PSX.EXE':'SLUS_001.58','GAME.EXE':'GAME.EXE','OPEN.EXE':'OPEN.EXE','END.EXE':'END.EXE'}
def load(p):
    b=open(p,'rb').read(); t,ts=struct.unpack_from('<2I',b,0x18); return t,b[0x800:0x800+ts]
JI={k:load(f'{R}/disc/jp/exe/{v}') for k,v in J.items()}
UI={k:load(f'{R}/disc/us/exe/{v}') for k,v in U.items()}
def tsv(p):
    hdr=None
    for l in open(p):
        if l.startswith('#'): continue
        r=l.rstrip('\n').split('\t')
        if hdr is None: hdr=r; continue
        yield dict(zip(hdr,r))
# ---- functions
fm={}
for r in csv.DictReader(open(f'{S}/fmap_final.tsv'),delimiter='\t'):
    if not r['jp_va'] or r['status']=='removed': continue
    fm[(r['image'],int(r['jp_va'],16))]=(int(r['jp_size'],16),int(r['us_va'],16),int(r['us_size'],16),r['status'])
# ---- data anchors from relocations
pairs=pickle.load(open(f'{S}/datapairs.pkl','rb'))
anch=collections.defaultdict(dict)
for (img,reg),c in pairs.items():
    for (j,u),n in c.items(): anch[img][j]=u-j
akeys={k:sorted(v) for k,v in anch.items()}
items={}
for r in tsv(f'{S}/jpcfg/data.tsv'):
    items[(r['image'],int(r['va'],16))]=max(int(r['size'],16),1)
for r in tsv(f'{S}/jpcfg/data_identities.tsv'):
    k=(r['image'],int(r['va'],16)); items[k]=max(items.get(k,0),int(r['size'],16),1)
def jbytes(img,va,n):
    t,b=JI[img]; o=va-t; return b[o:o+n] if 0<=o and o+n<=len(b) else None
CODE={}
for (img,va),(n,u,us,st) in fm.items():
    lo,hi=CODE.get(img,(va,va+n)); CODE[img]=(min(lo,va),max(hi,va+n))
IMM={1,4,5,6,7,8,9,10,11,12,13,14,15,32,33,34,35,36,37,38,40,41,42,43,46,50,58}
def norm(bs,va,img=None):
    if bs is None: return None
    b=bytearray(bs)
    code=img is not None
    for o in range((-va)%4,len(b)-3,4):
        w=int.from_bytes(b[o:o+4],'little')
        if 0x80010000<=w<0x80200000: b[o:o+4]=b'PPPP'; continue
        if code:
            op=w>>26
            if op in (2,3): w&=0xfc000000
            elif op in IMM: w&=0xffff0000
            b[o:o+4]=w.to_bytes(4,'little')
    return bytes(b)
def incode(img,va):
    lo,hi=CODE[img]; return lo<=va<hi
def ubytes(img,va,n):
    t,b=UI[img]; o=va-t; return b[o:o+n] if 0<=o and o+n<=len(b) else None
dm={}
for (img,va),n in sorted(items.items()):
    if (img,va) in fm: continue
    inside={anch[img][a] for a in akeys.get(img,[]) if va<=a<va+n}
    if len(inside)==1: d=inside.pop(); st='anchored'
    elif len(inside)>1: dm[(img,va)]=(n,None,n,'conflict'); continue
    else:
        L=akeys.get(img,[]); i=bisect.bisect_left(L,va)
        cands=[]
        if i>0: cands.append(anch[img][L[i-1]])
        if i<len(L): cands.append(anch[img][L[i]])
        F=sorted((v,dd) for (im,v),(nn,uu,us2,st2) in fm.items() if im==img and st2=='exact' for dd in [uu-v])
        fv=[x[0] for x in F]; k=bisect.bisect_left(fv,va)
        if jbytes(img,va,n) is not None and incode(img,va):
            if k>0: cands.append(F[k-1][1])
            if k<len(F): cands.append(F[k][1])
        if jbytes(img,va,n) is not None:
            cands.append(0)  # unmoved: checked against the bytes like any other candidate
        cands=list(dict.fromkeys(cands))
        jb=jbytes(img,va,n)
        if jb is not None:
            ci=img if incode(img,va) else None
            ok=[d for d in cands if norm(ubytes(img,va+d,n),va+d,ci)==norm(jb,va,ci)]
            if len(ok)==1: d=ok[0]; st='bytes'
            elif len(cands)==1: d=cands[0]; st='neighbour-bytes-differ'
            elif ok: d=ok[0]; st='bytes-ambiguous'
            else:
                F=sorted((v,vv+nn,uu-v) for (im,v),(nn,uu,us2,st2) in fm.items() if im==img and st2=='exact' for vv in [v])
                prevf=[x for x in F if x[1]<=va]; nextf=[x for x in F if x[0]>=va+n]
                if prevf and nextf and prevf[-1][2]==nextf[0][2]: d=prevf[-1][2]; st='code-gap'
                else: dm[(img,va)]=(n,None,n,'unresolved'); continue
        else:
            if len(cands)==1: d=cands[0]; st='neighbour'
            else: dm[(img,va)]=(n,None,n,'unresolved-bss'); continue
    if st=='anchored' and jbytes(img,va,n) is not None and norm(ubytes(img,va+d,n),va+d,img if incode(img,va) else None)!=norm(jbytes(img,va,n),va,img if incode(img,va) else None): st='anchored-bytes-differ'
    dm[(img,va)]=(n,va+d,n,st)
out=open(f'{S}/addrmap.tsv','w'); out.write('image\tkind\tjp_va\tjp_size\tus_va\tus_size\tstatus\n')
for kind,M in (('func',fm),('data',dm)):
    for (img,va),(n,u,us,st) in sorted(M.items()):
        out.write(f"{img}\t{kind}\t{va:#x}\t{n:#x}\t{'' if u is None else hex(u)}\t{us:#x}\t{st}\n")
c=collections.Counter((img,k,st) for k,M in (('func',fm),('data',dm)) for (img,va),(n,u,us,st) in M.items())
for k in sorted(c): print(k,c[k])
