# JP->US function placement, masking only relocation fields recorded in relocs.tsv.
import sys,struct,collections,bisect
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
M=collections.defaultdict(dict)   # image -> site_va -> mask
for r in tsv(f'{S}/jpcfg/relocs.tsv'):
    img=r['image']; k=r['kind']
    if k=='mips26': M[img][int(r['site_va'],16)]=0xfc000000
    elif k=='mips_hi16_lo16':
        M[img][int(r['site_va'],16)]=0xffff0000
        if r['paired_site_va']: M[img][int(r['paired_site_va'],16)]=0xffff0000
vend={(r['image'],int(r['va'],16)) for r in tsv(f'{S}/jpcfg/functions_vendored.tsv')}
names={(r['image'],int(r['va'],16)):r['name'] for r in tsv(f'{S}/jpcfg/function_identities.tsv')}
rows=[r for r in tsv(f'{S}/jpcfg/functions.tsv') if r['kind']=='function']
out=open(f'{S}/jp_us_map.tsv','w'); out.write('image\tjp_va\tsize\tus_va\tdelta\tstatus\tvendored\tname\n')
for img in U:
    jt,jb=load(f'{R}/disc/jp/exe/{img}'); ut,ub=load(f'{R}/disc/us/exe/{U[img]}')
    uw=[struct.unpack_from('<I',ub,o)[0] for o in range(0,len(ub)-3,4)]
    F=sorted((int(r['va'],16),int(r['size'],16),r) for r in rows if r['image']==img)
    c=collections.Counter(); prevd=0
    for va,size,r in F:
        jo=va-jt
        IMM={8,9,10,11,12,13,14,32,33,34,35,36,37,38,40,41,42,43,46,50,58}
        def mk(w):
            op=w>>26
            if op in (2,3): return 0xfc000000
            if op==15: return 0xffff0000
            if op in IMM and (w>>21)&31: return 0xffff0000
            return 0xffffffff
        masks=[mk(struct.unpack_from('<I',jb,jo+i)[0]) for i in range(0,size,4)]
        sig=[struct.unpack_from('<I',jb,jo+i)[0]&m for i,m in zip(range(0,size,4),masks)]
        n=len(sig)
        def ok(o):
            k=o//4
            return k+n<=len(uw) and all((uw[k+i]&masks[i])==sig[i] for i in range(n))
        # search near the running delta first, then the whole image
        near=[o for o in range(max(0,(jo+prevd-0x6000))&~3,min(len(ub)-size,jo+prevd+0x6000),4) if ok(o)]
        cands=near or [o for o in range(0,len(ub)-size,4) if (uw[o//4]&masks[0])==sig[0] and ok(o)]
        if cands:
            o=min(cands,key=lambda o:abs((o+ut-va)-prevd)); d=o+ut-va
            st='same' if len(cands)==1 else 'same-ambiguous'
            if st=='same' or size>=16: prevd=d
            uva=hex(o+ut); ds=hex(d)
        else: st='changed'; uva=''; ds=''
        v=(img,va) in vend; c[(st,v)]+=1
        out.write(f"{img}\t{va:#x}\t{size:#x}\t{uva}\t{ds}\t{st}\t{int(v)}\t{names.get((img,va),r['name'])}\n")
    print(img,{f"{s}{'/sdk' if v else '/game'}":n for (s,v),n in sorted(c.items())})
