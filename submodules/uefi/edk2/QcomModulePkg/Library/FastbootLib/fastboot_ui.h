/*
 * Lightweight software-rendered UI for the fastboot screen.
 * All entry points are defensive: if no graphics output is available they
 * return without touching anything, so the boot/fastboot path is never blocked.
 */
#ifndef FASTBOOT_UI_H
#define FASTBOOT_UI_H

#include <Uefi.h>

/* Silky fade/slide-in animation played right before fastboot starts. */
VOID FastbootUiEntryTransition (VOID);

/* Draw the settled fastboot screen once (called when fastboot is ready). */
VOID FastbootUiDrawMain (VOID);

#endif /* FASTBOOT_UI_H */
