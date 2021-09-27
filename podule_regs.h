#ifndef PODULE_REGS_H
#define PODULE_REGS_H

/* From the view of the host: */
#define PR_BASE         0x2000
#define PR_ROM_PAGE_BASE        0x1000

/* From the view of us, the podule: */
#define PR_OFFSET       (PR_BASE/4)
#define PR_ROM_PAGE_OFFSET      (PR_ROM_PAGE_BASE/4)

/* Page regs: max 15 bits of 1KB page, i.e. 32M max ROM area. */
#define PR_PAGE_L       0
#define PR_PAGE_H       1       // Host sets bit 7 to load; podule clears

/* Reset: incremented by loader/OS */
#define PR_RESET        2

/* Packet descriptors: Arc sees these byte-wide, but we can access as words: */
#define PR_TX_DESCR(pb, desc_n) ( ((volatile uint32_t *)(&(pb)[PR_TX0_0]))[desc_n] )
#define PR_RX_DESCR(pb, desc_n) ( ((volatile uint32_t *)(&(pb)[PR_RX0_0]))[desc_n] )

#define PR_DESCR_READY          0x80000000
#define PR_DESCR_ADDR_MASK      0x000001ff
#define PR_DESCR_ADDR_SHIFT     0
#define PR_DESCR_SIZE_MASK      0x001ff000
#define PR_DESCR_SIZE_SHIFT     12
#define PR_DESCR_CID_MASK       0x7f000000
#define PR_DESCR_CID_SHIFT      24

#define PR_DESCR_IS_READY(x)    ( !!((x) & PR_DESCR_READY) )
#define PR_DESCR_ADDR(x)        ( ((x) & PR_DESCR_ADDR_MASK) >> PR_DESCR_ADDR_SHIFT )
#define PR_DESCR_SIZE(x)        ( ((x) & PR_DESCR_SIZE_MASK) >> PR_DESCR_SIZE_SHIFT )
#define PR_DESCR_CID(x)         ( ((x) & PR_DESCR_CID_MASK) >> PR_DESCR_CID_SHIFT )

#define PR_TX_TAIL      0x40
#define PR_RX_HEAD      0x41

#define PR_TX0_0        0x80
#define PR_TX0_1        0x81
#define PR_TX0_2        0x82
#define PR_TX0_3        0x83
#define PR_TX1_0        0x84
#define PR_TX1_1        0x85
#define PR_TX1_2        0x86
#define PR_TX1_3        0x87
#define PR_TX2_0        0x88
#define PR_TX2_1        0x89
#define PR_TX2_2        0x8a
#define PR_TX2_3        0x8b
#define PR_TX3_0        0x8c
#define PR_TX3_1        0x8d
#define PR_TX3_2        0x8e
#define PR_TX3_3        0x8f
#define PR_TX4_0        0x90
#define PR_TX4_1        0x91
#define PR_TX4_2        0x92
#define PR_TX4_3        0x93
#define PR_TX5_0        0x94
#define PR_TX5_1        0x95
#define PR_TX5_2        0x96
#define PR_TX5_3        0x97
#define PR_TX6_0        0x98
#define PR_TX6_1        0x99
#define PR_TX6_2        0x9a
#define PR_TX6_3        0x9b
#define PR_TX7_0        0x9c
#define PR_TX7_1        0x9d
#define PR_TX7_2        0x9e
#define PR_TX7_3        0x9f

#define PR_RX0_0        0xa0
#define PR_RX0_1        0xa1
#define PR_RX0_2        0xa2
#define PR_RX0_3        0xa3
#define PR_RX1_0        0xa4
#define PR_RX1_1        0xa5
#define PR_RX1_2        0xa6
#define PR_RX1_3        0xa7
#define PR_RX2_0        0xa8
#define PR_RX2_1        0xa9
#define PR_RX2_2        0xaa
#define PR_RX2_3        0xab
#define PR_RX3_0        0xac
#define PR_RX3_1        0xad
#define PR_RX3_2        0xae
#define PR_RX3_3        0xaf
#define PR_RX4_0        0xb0
#define PR_RX4_1        0xb1
#define PR_RX4_2        0xb2
#define PR_RX4_3        0xb3
#define PR_RX5_0        0xb4
#define PR_RX5_1        0xb5
#define PR_RX5_2        0xb6
#define PR_RX5_3        0xb7
#define PR_RX6_0        0xb8
#define PR_RX6_1        0xb9
#define PR_RX6_2        0xba
#define PR_RX6_3        0xbb
#define PR_RX7_0        0xbc
#define PR_RX7_1        0xbd
#define PR_RX7_2        0xbe
#define PR_RX7_3        0xbf

#define PR_RX_TX_BUFSZ  512
#define PR_TX_BUFFERS   0x400
#define PR_RX_BUFFERS   0x600

#endif
