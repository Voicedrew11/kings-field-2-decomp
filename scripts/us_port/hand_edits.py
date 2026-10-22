"""Hand edits applied after scripts/us_port/retarget.py (US END/OPEN structure, US-only placeholders)."""
import re,subprocess
def sub(p,a,b):
    s=open(p).read(); assert a in s,(p,a[:70]); s=s.replace(a,b,1); open(p,'w').write(s)
p='src/end/main.c'; s=open(p).read()
s=s[:s.index('enum {\n    ENDING_DATA_BYTES')]+'''DATA(0x8003975c, 0x4, ".sdata")
u8 *overlay_next_request = (u8 *)0x800102f0;

/* US END: plays ED0.S, then ED1.S, then waits forever. */
ADDRESS(0x800119a4, 0xc4)
void main(void)
{
    ResetCallback();
    InitHeap((void *)ENDING_HEAP_BASE, ENDING_HEAP_BYTES);
    CdInit();
    PadInit(0);
    ExitCriticalSection();
    audio_initialize();
    display_initialize();
    display_current = &display_buffers[0];
    PutDrawEnv(&display_current->draw);
    PutDispEnv(&display_current->disp);
    display_current = &display_buffers[1];
    PutDrawEnv(&display_current->draw);
    PutDispEnv(&display_current->disp);
    ending_play_movie(0);
    ending_play_movie(1);
    for (;;) {
    }
}
'''
s=s.replace('#include <kf/lib/cd_file.h>\n','')
open(p,'w').write(s)
sub('include/kf/end/ending.h','''/* END.EXE: loads ED.D, opens its music and plays the ending movie. */
extern u8 *ending_data;
extern char ending_data_file[5];

void ending_load_data(void);
void ending_open_audio(void);
void ending_play_movie(void);''','''/* US END.EXE: plays the two ending movies, ED0.S and ED1.S. */
void ending_play_movie(s32 movie);''')
p='src/end/movie.c'; s=open(p).read()
s=s.replace('RODATA(0x8001100c, 0x1f)','RODATA(0x80011000, 0x2b)')
a='''/* The tutorial's anim() as adapted for END.EXE: decode the ending movie while
 * fading the music, then stop the drive and wait forever. */
ADDRESS(0x80011cc4, 0x1dc)
void ending_play_movie(void)
{
    CdlFILE file;
    u_char mode;
    int volume = ENDING_SEQUENCE_VOLUME;

    SsSetMVol(ENDING_MASTER_VOLUME, ENDING_MASTER_VOLUME);
    SsSeqSetVol(audio_sequence_id, ENDING_SEQUENCE_VOLUME, ENDING_SEQUENCE_VOLUME);
    SsSeqPlay(audio_sequence_id, SSPLAY_PLAY, 1);
    if (CdSearchFile(&file, "\\\\OP\\\\ED.S;1") == NULL) {
        printf("\\n__ file not found");
        return;
    }
    SsSetMVol(ENDING_MASTER_VOLUME, ENDING_MASTER_VOLUME);
    SsSeqSetVol(audio_sequence_id, ENDING_SEQUENCE_VOLUME, ENDING_SEQUENCE_VOLUME);
    SsSeqPlay(audio_sequence_id, SSPLAY_PLAY, SSPLAY_INFINITY);'''
assert a in s; s=s.replace(a,'''/* TODO(us-port): not yet matched against US END; JP body with the sequence
 * audio removed, as US END links no LIBSND sequence player. */
ADDRESS(0x80011cc4, 0x1dc)
void ending_play_movie(s32 movie)
{
    CdlFILE file;
    u_char mode;

    if (CdSearchFile(&file, movie == 0 ? "\\\\OP\\\\ED0.S;1" : "\\\\OP\\\\ED1.S;1") == NULL) {
        printf("\\n__ file not found");
        return;
    }''')
a='''        if (StrFrame >= ENDING_FADE_FRAME && volume > 0) {
            volume--;
            SsSeqSetVol(audio_sequence_id, volume, volume);
            if (volume == 0) {
                SsSeqStop(audio_sequence_id);
                SsSeqSetVol(audio_sequence_id, ENDING_SEQUENCE_VOLUME, ENDING_SEQUENCE_VOLUME);
                SsSeqPlay(audio_sequence_id, SSPLAY_PLAY, 1);
            }
        }
'''
assert a in s; s=s.replace(a,'')
a='    display_current = &display_buffers[0];\n    SsSeqStop(audio_sequence_id);\n}'
assert a in s; s=s.replace(a,'    display_current = &display_buffers[0];\n}')
open(p,'w').write(s)
s=open('config/units.toml').read()
for u in ('end.audio','end.cd_file'):
    s,n=re.subn(r'\n\[\[unit\]\]\nunit = "%s"\n(?:[^\[\n][^\n]*\n)*'%re.escape(u),'\n',s); assert n==1,u
open('config/units.toml','w').write(s)
sub('src/lib/cd_file.c','DATA_AT("END", 0x8003aa40, 0x5, ".sdata")\n','')
sub('src/lib/cd_file.c','DATA_AT("END", 0x8003aa48, 0x3, ".sdata")\n','')
sub('src/lib/cd_file.c','ADDRESS_AT("END", 0x80011e64, 0x15c)\n','')
sub('src/lib/audio_init.c','DATA_AT("END", 0x800a93f8, 0x158, ".bss")\n','')
s=open('src/open/main.c').read()
s,n=re.subn(r'DATA\(0x8003db8c, 0x5, "\.sdata"\)\nchar opening_data_file\[5\] = "OP\.D";\n','',s); assert n==1
s=s.replace('cd_file_load_into((u_long *)opening_data, opening_data_file)','cd_file_load_into((u_long *)opening_data, "OPU.D")')
open('src/open/main.c','w').write(s)
sub('include/kf/open/opening.h','extern char opening_data_file[5];\n','')
PH='''/* TODO(us-port): US-only function, not in the JP build; not reconstructed. */
ADDRESS({va}, {size})
void func_{name}(void)
{{
}}

'''
sub('src/game/menu_root.c','ADDRESS(0x8001912c, 0xd8)\n',PH.format(va='0x80018e80',size='0x2ac',name='80018e80')+'ADDRESS(0x8001912c, 0xd8)\n')
sub('src/game/menu_root.c','ADDRESS(0x800197d4, ',PH.format(va='0x800196c8',size='0x10c',name='800196c8')+'ADDRESS(0x800197d4, ')
subprocess.run(['git','rm','-q','--ignore-unmatch','src/end/audio.c'],check=True)
