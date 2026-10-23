#include <kf/lib/address.h>
#include <kf/lib/audio.h>

enum {
    AUDIO_REVERB_DEPTH = 0x20,
    AUDIO_SERIAL_VOLUME = 0x7f
};

DATA_AT("OPEN", 0x800ad1b0, 0x158, ".bss")
char audio_sequence_table[SS_SEQ_TABSIZ * KF_AUDIO_SEQUENCE_CAPACITY * KF_AUDIO_TRACKS_PER_SEQUENCE];

ADDRESS_AT("OPEN", 0x800122bc, 0x8c)
ADDRESS_AT("END", 0x80011ba8, 0x4c)
void audio_initialize(void)
{
    SsInit();
#ifdef KF_END
    SsSetSerialAttr(SS_SERIAL_A, SS_MIX, SS_SON);
    SsSetSerialVol(SS_SERIAL_A, AUDIO_SERIAL_VOLUME, AUDIO_SERIAL_VOLUME);
    SsSetMVol(0x7f, 0x7f);
#else
    SsSetTickMode(SS_TICK60);
    SsSetTableSize(audio_sequence_table, KF_AUDIO_SEQUENCE_CAPACITY, KF_AUDIO_TRACKS_PER_SEQUENCE);
    SsUtSetReverbType(SS_REV_TYPE_HALL);
    SsUtReverbOn();
    SsUtSetReverbDepth(AUDIO_REVERB_DEPTH, AUDIO_REVERB_DEPTH);
    SsStart2();
    SsSetMVol(0, 0);
    SsSetSerialAttr(SS_SERIAL_A, SS_MIX, SS_SON);
    SsSetSerialVol(SS_SERIAL_A, AUDIO_SERIAL_VOLUME, AUDIO_SERIAL_VOLUME);
#endif
}
