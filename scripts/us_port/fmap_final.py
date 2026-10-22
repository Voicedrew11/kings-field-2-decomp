import csv,sys
S=sys.argv[1]
rows=list(csv.DictReader(open(f'{S}/jp_us_map.tsv'),delimiter='\t'))
runs={}
for l in open(f'{S}/runs_ok.tsv'):
    img,jv,js,uv,us=l.split(); runs[(img,int(jv,16))]=(int(uv,16),int(us,16))
OV={  # hand-checked: (image, jp_va) -> (us_va, us_size, status)
 ('GAME.EXE',0x8001876c):(0x8001883c,0x644,'changed'),
 ('PSX.EXE',0x80010038):(0x80010038,0xe8,'changed'),
 ('OPEN.EXE',0x80011ac0):(0x80011ae0,0x4e0,'changed'),
 ('END.EXE',0x800119f8):(0x800119a4,0xc4,'changed'),
 ('END.EXE',0x80011b40):(0x80011a68,0x140,'changed'),
 ('END.EXE',0x80011c80):(0x80011ba8,0x4c,'changed'),
 ('END.EXE',0x80011ad8):(None,0,'removed'),
 ('END.EXE',0x80011cec):(None,0,'removed'),
 ('END.EXE',0x80011e64):(None,0,'removed'),
 ('END.EXE',0x80011fc0):(0x80011cac,0x8,'exact'),
 ('END.EXE',0x8001c02c):(0x80021c28,0x20,'exact'),  # its jal reaches 0x800262c4, the US copy of 0x80027548
}
NEW=[('GAME.EXE',0x80018e80,0x2ac,'us-only'),('GAME.EXE',0x800196c8,0x10c,'us-only'),('OPEN.EXE',0x80012638,0x6f0,'us-only')]
out=open(f'{S}/fmap_final.tsv','w'); out.write('image\tjp_va\tjp_size\tus_va\tus_size\tstatus\tvendored\tname\n')
import collections; c=collections.Counter()
for r in rows:
    img=r['image']; jv=int(r['jp_va'],16); js=int(r['size'],16)
    if (img,jv) in OV: uv,us,st=OV[(img,jv)]
    elif r['us_va']: uv,us,st=int(r['us_va'],16),js,'exact'
    elif (img,jv) in runs: (uv,us),st=runs[(img,jv)],'changed'
    else: uv,us,st=None,0,'removed'
    c[(img,st,r['vendored'])]+=1
    out.write(f"{img}\t{jv:#x}\t{js:#x}\t{'' if uv is None else hex(uv)}\t{us:#x}\t{st}\t{r['vendored']}\t{r['name']}\n")
for img,uv,us,st in NEW:
    out.write(f"{img}\t\t0x0\t{uv:#x}\t{us:#x}\t{st}\t0\tfunc_{uv:08x}\n"); c[(img,st,'0')]+=1
for k in sorted(c): print(k,c[k])
