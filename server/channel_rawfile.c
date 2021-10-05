/* channel_rawfile
 *
 * World's simplest host file access:  responds to messages to open a file,
 * access blocks, and close the file.
 *
 * Supports basic file type mapping (using filename suffixes in the host FS).
 *
 * 1 Oct 2021  ME
 *
 * MIT License
 *
 * Copyright (c) 2021 Matt Evans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <inttypes.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <strings.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <glob.h>
#include <limits.h>

#include "channels.h"


#define DEBUG   3


struct init_read_response {
        uint8_t  success;
        uint8_t  pad1[3];
        uint32_t filesize;
        uint32_t load;
        uint32_t exec;
};

struct read_block_request {
        uint8_t  opcode;
        uint8_t  pad1[3];
        uint32_t offset;
        uint32_t size;
};

static int     current_file = -1;
static uint8_t read_buff[512];

void            channel_rawfile_init(void)
{
        if (current_file != -1)
                close(current_file);
        current_file = -1;
}

/* Acorn time is 40-bit, centiseconds from midnight 1 Jan 1900.
 * Convert UNIX time to Acorn time.
 */
static  uint64_t crf_atime_from_time_t(time_t t)
{
        // FIXME: This is probably imprecise/wrong, FWIW.
        uint64_t at = (t + (70 * 365.2425 * 24 * 60 * 60))*100;
        return at;
}

static  void    crf_create_type(uint16_t type, time_t timestamp,
                                uint32_t *load, uint32_t *exec)
{
        uint64_t at = crf_atime_from_time_t(timestamp);

        *load = 0xfff00000 | ((type & 0xfff) << 8) | ((at >> 32) & 0xff);
        *exec = at & 0xffffffff;
}

// Turn a filename in format 'filename,([0-9a-f]{3})' into a numeric type:
static  uint16_t crf_parse_type(char *pathname)
{
        char *c = rindex(pathname, ',');
        uint16_t ft = 0xffd;                            // Default: Data
        if (c) {
                c++;
                ft = strtoul(c, NULL, 16);
        }
        return ft;
}

// Turn a filename in format filename,([0-9a-f]{1,7}-[0-9a-f]{1,7}) into load/exec:
static  void    crf_parse_lx(char *pathname, uint32_t *load, uint32_t *exec)
{
        // This is a quick hack, not very robust to corners/mis-parsing!
        char *c = rindex(pathname, ',');
        if (c) {
                char *n = 0;
                c++;
                *load = strtoul(c, &n, 16);
                if (n && *n == '-')
                        *exec = strtoul(n + 1, NULL, 16);
        }
}

/* Opens the given filename for reading.
 * Looks for ",xxx" and ",xxxx-xxxx" alternative files to get type/load/exec
 * metadata; returns into load/exec parameters.  (Same format as HostFS, FWIW.)
 */
static int      crf_open_read(char *filename, uint32_t *load, uint32_t *exec)
{
        if (current_file != -1)
                close(current_file);

        printf("+++ Opening '%s'\n", filename);

        // Default Arc file attributes:
        uint16_t ftype = 0xffff;                        // If <= 0xfff, filetype
        *load = 0xfff00000 | (0xffd << 8);              // Filetype: Data
        *exec = 0;

        char *ofn = filename;
        /* Do some globbing to match one of:
         * - filename,([0-9a-f]{3})                     (has filetype)
         * - filename,([0-9a-f]{1,7}-[0-9a-f]{1,7})     (has load/exec)
         * - filename
         */
        glob_t  gt;
        int     glob_free = 0;
        char    pathname[PATH_MAX];

        snprintf(pathname, PATH_MAX, "%s,[0-9a-f][0-9a-f][0-9a-f]", filename);
        if (glob(pathname, GLOB_MARK, NULL, &gt) == 0) {
                glob_free = 1;
                // Just take first match
                if (gt.gl_pathc > 1) {
                        printf("Warning: Multiple matches for '%s'\n",
                               pathname);
                }
                strncpy(pathname, gt.gl_pathv[0], PATH_MAX);
                ofn = pathname;

                ftype = crf_parse_type(pathname);
#if DEBUG > 1
                printf("(Found %s with filetype 0x%x)\n", gt.gl_pathv[0], ftype);
#endif
        } else {
                printf("(Can't find fname alternative with type)\n");
                // One more try, for load/exec format:
                snprintf(pathname, PATH_MAX,
                         "%s,[0-9a-f]*-[0-9a-f]*", filename);
                if (glob(pathname, GLOB_MARK, NULL, &gt) == 0) {
                        glob_free = 1;
                        if (gt.gl_pathc > 1) {
                                printf("Warning: Multiple matches for '%s'\n",
                                       pathname);
                        }
                        strncpy(pathname, gt.gl_pathv[0], PATH_MAX);
                        ofn = pathname;

                        crf_parse_lx(pathname, load, exec);
#if DEBUG > 1
                        printf("(Found %s with load 0x%x/exec 0x%x)\n",
                               gt.gl_pathv[0], *load, *exec);
#endif
                } else {
                        printf("(Can't find fname alternative with l/x)\n");
                }
        }
        if (glob_free)
                globfree(&gt);
        // Else, just try given pathname.

        current_file = open(ofn, O_RDONLY);

        if (current_file < 0) {
                current_file = -1;
                perror("--- File open for read");
                return errno;
        }

        if (ftype <= 0xfff) {
                // File has a type.  Create attributes, together with mtime:
                struct stat sb;
                fstat(current_file, &sb);
                crf_create_type(ftype, sb.st_mtime, load, exec);
        }

        return 0;
}

void            channel_rawfile_rx(uint8_t *data, unsigned int len)
{
        if (data[0] == CID_RAWFILE_INIT_READ) {
#if DEBUG > 1
                printf("+++ raw file request (%d)\n", data[0]);
#endif
                uint32_t load, exec;
                int r = crf_open_read(&data[1], &load, &exec);

                struct init_read_response response;
                response.success = r;

                if (r >= 0) {
                        struct stat sb;

                        fstat(current_file, &sb);
                        response.filesize = htole32(sb.st_size);
                        response.load = load;
                        response.exec = exec;
                }

                send_packet(CID_RAWFILE, sizeof(response), (uint8_t *)&response);
        } else if (data[0] == CID_RAWFILE_READ_BLOCK) {
                struct read_block_request *rbr =
                        (struct read_block_request *)data;

                uint32_t offset = le32toh(rbr->offset);
                uint32_t size = le32toh(rbr->size);
#if DEBUG > 2
                printf("+++ Read block (%d) offset %d, size %d\n",
                       data[0], offset, size);
#endif
                if (current_file != -1) {
                        pread(current_file, read_buff, size, offset);
                        send_packet(CID_RAWFILE, size, read_buff);
                } else {
                        printf("--- No file open, ignoring request!\n");
                }
        } else if (data[0] == CID_RAWFILE_CLOSE) {
#if DEBUG > 1
                printf("+++ Closing file\n");
#endif
                if (current_file != -1)
                        close(current_file);
                current_file = -1;
        } else {
                printf("rawfile: Odd byte 0: 0x%x\n", data[0]);
        }
}
