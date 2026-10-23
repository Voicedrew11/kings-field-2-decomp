#ifndef KF_PSYQ_KERNEL_H
#define KF_PSYQ_KERNEL_H

/*
 * Psy-Q 3.0 kernel, controller, and BIOS file interfaces. KERNEL.H
 * and SYS/FILE.H provide the vendor constants and layouts, but this release
 * ships no LIBAPI header with callable declarations. Keep the missing BIOS
 * entry points here so game sources do not reproduce private extern blocks.
 */

/* ASM.H supplies NREGS for KERNEL.H's C layouts. The native preprocessor
 * does not supply the SDK driver's LANGUAGE_C selection. */
#ifndef LANGUAGE_C
#define LANGUAGE_C 1
#endif
#include <ASM.H>
#include <KERNEL.H>
#include <SYS/FILE.H>

extern long OpenEvent(
    unsigned long descriptor, long spec, long mode, void (*handler)(void));
extern long EnableEvent(long event);
extern long DisableEvent(long event);
extern long TestEvent(long event);
extern long UnDeliverEvent(long event);
extern long CloseEvent(long event);

extern void EnterCriticalSection(void);
extern void ExitCriticalSection(void);
extern void ResetCallback(void);
extern void StopCallback(void);

extern void InitHeap(void *head, long size);
extern void InitCARD(long pad_enable);
extern long StartCARD(void);
extern long StopCARD(void);
extern void ChangeClearPAD(long value);
extern void InitCARD2(long pad_enable);
extern void StartCARD2(void);
extern void StopCARD2(void);
extern void _bu_init(void);
extern void _card_auto(long enable);
extern long _card_info(long channel);
extern void _new_card(void);

extern unsigned long PAD_init2(unsigned long mode, unsigned long *pad_state);
extern void PAD_dr(void);
extern void StopPAD2(void);

extern long open(const char *name, long mode);
extern long close(long file);
extern long lseek(long file, long offset, long origin);
extern long read(long file, void *buffer, long length);
extern long write(long file, const void *buffer, long length);
extern long erase(const char *name);
#if defined(__cplusplus)
/* delete is a C++ keyword; bind the BIOS entry point by its linked name. */
extern long bios_delete(const char *name) __asm__("delete");
#else
extern long delete(const char *name);
#define bios_delete delete
#endif
extern long format(const char *device);
extern struct DIRENTRY *firstfile(const char *pattern, struct DIRENTRY *entry);
extern struct DIRENTRY *nextfile(struct DIRENTRY *entry);

extern long _96_init(void);
extern long _96_remove(void);
extern long Load(const char *name, struct EXEC *header);
extern long Exec(struct EXEC *header, long argc, char **argv);
/* LIBAPI C159: selects the 2 or 8 MB RAM map. */
extern void SetMem(long megabytes);

#endif
