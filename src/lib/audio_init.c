#include <kf/lib/address.h>
#include <kf/lib/audio.h>

enum {
    AUDIO_REVERB_DEPTH = 0x20,
    AUDIO_SERIAL_VOLUME = 0x7f,
    AUDIO_MASTER_VOLUME_MAX = 0x7f
};

/* US END links no sequence player, so only OPEN owns the table. */
#ifdef KF_END
/* Unreferenced: retail END reserves an exported COMMON slot between
 * _svm_stereo_mono and the screen buffers, where JP END kept its VAB id.
 * Its name and type are unknown; this one fits PSYLINK's bucket order. */
DATA_AT("END", 0x800a1eec, 0x2, ".bss")
s16 end_vab;
#else
DATA_AT("OPEN", 0x800ad1b0, 0x158, ".bss")
char seq_table[SS_SEQ_TABSIZ * KF_AUDIO_SEQUENCE_CAPACITY * KF_AUDIO_TRACKS_PER_SEQUENCE];
#endif

ADDRESS_AT("OPEN", 0x800122bc, 0x8c)
ADDRESS_AT("END", 0x80011ba8, 0x4c)
void audio_initialize(void)
{
    SsInit();
#ifdef KF_END
    SsSetSerialAttr(SS_SERIAL_A, SS_MIX, SS_SON);
    SsSetSerialVol(SS_SERIAL_A, AUDIO_SERIAL_VOLUME, AUDIO_SERIAL_VOLUME);
    SsSetMVol(AUDIO_MASTER_VOLUME_MAX, AUDIO_MASTER_VOLUME_MAX);
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
