#ifndef PODULE_REGS_H
#define PODULE_REGS_H

/* From the view of the host: */
#define PR_BASE         0x2000

/* From the view of us, the podule: */
#define PR_OFFSET       (PR_BASE/4)

/* Page regs: max 15 bits of 1KB page, i.e. 32M max ROM area. */
#define PR_PAGE_L       0
#define PR_PAGE_H       1       // Host sets bit 7 to load; podule clears

/* Reset: incremented by loader/OS */
#define PR_RESET        2

#endif
