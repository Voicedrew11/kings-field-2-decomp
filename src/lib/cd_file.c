#include <kf/lib/address.h>
#include <kf/lib/cd_file.h>
#include <kf/lib/null.h>
#include <psyq/cd.h>
#include <psyq/libc.h>

enum {
    KF_CD_SECTOR_BYTES = 0x800,
    KF_CD_SECTOR_WORDS = KF_CD_SECTOR_BYTES / sizeof(u_long),
    KF_CD_SECTOR_SHIFT = 11,
    KF_CD_DATA_START_SECOND = 2
};

DATA_AT("OPEN", 0x8003e740, 0x5, ".sdata")
DATA_AT("END", 0x8003aa40, 0x5, ".sdata")
char cd_path_prefix[5] = "\\OP\\";
DATA_AT("OPEN", 0x8003e748, 0x3, ".sdata")
DATA_AT("END", 0x8003aa48, 0x3, ".sdata")
char cd_version_suffix[3] = ";1";

/* Reads \OP\<relative_path>;1 sector by sector into DESTINATION. */
ADDRESS_AT("OPEN", 0x80013b5c, 0x15c)
ADDRESS_AT("END", 0x80011e64, 0x15c)
int cd_file_load_into(u_long *destination, const char *relative_path)
{
    char path[40];
    CdlFILE file;
    CdlLOC start;
    u_char mode;
    s32 sectors;

    start.minute = 0;
    start.second = KF_CD_DATA_START_SECOND;
    start.sector = 0;
    start.track = 0;
    memcpy((void *)path, (const void *)cd_path_prefix, sizeof cd_path_prefix);
    strcat(path, relative_path);
    strcat(path, cd_version_suffix);
    if (CdSearchFile(&file, path) == NULL) {
        return KF_CD_NOT_FOUND;
    }
    mode = CdlModeSpeed;
    sectors = (file.size + KF_CD_SECTOR_BYTES - 1) >> KF_CD_SECTOR_SHIFT;
    CdControl(CdlSetmode, &mode, NULL);
    CdControl(CdlReadN, (u_char *)&file.pos, NULL);
    while (--sectors != -1) {
        if (CdReady(0, NULL) != CdlDataReady) {
            CdControl(CdlSetloc, (u_char *)&start, NULL);
            CdPause();
            return KF_CD_READ_FAILED;
        }
        CdGetSector((void *)destination, KF_CD_SECTOR_WORDS);
        destination += KF_CD_SECTOR_WORDS;
    }
    CdControl(CdlSetloc, (u_char *)&start, NULL);
    CdPause();
    return KF_CD_LOADED;
}
