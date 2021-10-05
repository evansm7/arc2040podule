#ifndef CHANNELS_H
#define CHANNELS_H

// Channel types
#define CID_IGNORE                      0
#define CID_HOSTINFO                    1
#define CID_HOSTINFO_PROTO_VERSION      1
#define CID_HOSTINFO_STRING             "ArcPipePodule host server" // 28 max
#define CID_RAWFILE                     2
#define CID_RAWFILE_INIT_READ           0
#define CID_RAWFILE_READ_BLOCK          1
#define CID_RAWFILE_CLOSE               4

extern void     channel_hostinfo_rx(uint8_t *data, unsigned int len);

extern void     channel_rawfile_init(void);
extern void     channel_rawfile_rx(uint8_t *data, unsigned int len);

extern void     send_packet(unsigned int cid, unsigned int len, uint8_t *data);

#endif
