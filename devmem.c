#include <stdio.h> // *printf
#include <fcntl.h> // open
#include <string.h> // memcpy
#include <unistd.h> // close
#include <stdint.h> // uint8_t
#include <stdbool.h> // bool
#include <stdlib.h> // exit
#include <errno.h> // errno
#include <ctype.h> // isprint
#include <endian.h> // be*toh
#include <sys/mman.h> //mmap head file
#include <getopt.h> // struct option, getopt_long
#include <sys/stat.h> // struct stat, stat

enum LOG_LEVEL {
    LOG_LEVEL_UNKNOWN = -1,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_NOTICE,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_NUM,
};
static enum LOG_LEVEL log_level = LOG_LEVEL_WARNING;
static FILE *STDOUT = NULL;
static FILE *STDERR = NULL;

#define __LOG(fp, fmt, LEVEL, args...) \
    fprintf(fp, "%s: %s %s().L%d: " fmt, LEVEL, __FILE__, __FUNCTION__, __LINE__, ##args)
#define LOG(level, fmt, args...)                                    \
do {                                                                \
    enum LOG_LEVEL _level = (level);                                \
    FILE *_stdout, *_stderr, *fp;                                   \
    const char *LEVEL;                                              \
    if (_level > log_level) break;                                  \
    if (STDOUT) _stdout = STDOUT; else _stdout = stdout;            \
    if (STDERR) _stderr = STDERR; else _stderr = stderr;            \
    switch (_level) {                                               \
    case LOG_LEVEL_FATAL:   fp = _stderr; LEVEL = "FATAL"; break;   \
    case LOG_LEVEL_ERR:     fp = _stderr; LEVEL = "ERR"; break;     \
    case LOG_LEVEL_WARNING: fp = _stderr; LEVEL = "WARNING"; break; \
    case LOG_LEVEL_NOTICE:  fp = _stdout; LEVEL = "NOTICE"; break;  \
    case LOG_LEVEL_INFO:    fp = _stdout; LEVEL = "INFO"; break;    \
    case LOG_LEVEL_DEBUG:   fp = _stdout; LEVEL = "DEBUG"; break;   \
    default:                fp = _stderr; LEVEL = "UNKNOWN"; break; \
    }                                                               \
    __LOG(fp, fmt, LEVEL, ##args);                                  \
} while (0)

#define LOG_FATAL(fmt, args...)     LOG(LOG_LEVEL_FATAL,    fmt, ##args)
#define LOG_ERR(fmt, args...)       LOG(LOG_LEVEL_ERR,      fmt, ##args)
#define LOG_WARNING(fmt, args...)   LOG(LOG_LEVEL_WARNING,  fmt, ##args)
#define LOG_NOTICE(fmt, args...)    LOG(LOG_LEVEL_NOTICE,   fmt, ##args)
#define LOG_INFO(fmt, args...)      LOG(LOG_LEVEL_INFO,     fmt, ##args)
#define LOG_DEBUG(fmt, args...)     LOG(LOG_LEVEL_DEBUG,    fmt, ##args)

union multi_pointer {
    void *p;
    uint8_t *p8;
    uint16_t *p16;
    uint32_t *p32;
    uint64_t *p64;
};

enum RDWR_MODE {
    MODE_RD_ONLY,
    MODE_WR_ONLY,
    MODE_RD_WR,
    MODE_WR_RD,
    MODE_RD_WR_RD,
    MODE_NUM,
};

enum RDWR_WIDTH {
    WIDTH_BYTE = 1,
    WIDTH_HALF = 2 * WIDTH_BYTE,
    WIDTH_WORD = 2 * WIDTH_HALF,
    WIDTH_DWORD = 2 * WIDTH_WORD,
};

static const char *short_options = "f:o:w:t:s:n:ci:m:P:b:?hd:v";
static const struct option long_options[] = {
    {"file",                    required_argument,  NULL,   'f'},
    {"offset",                  required_argument,  NULL,   'o'},
    {"width",                   required_argument,  NULL,   'w'},
    {"step",                    required_argument,  NULL,   't'},
    {"size",                    required_argument,  NULL,   's'},
    {"number",                  required_argument,  NULL,   'n'},
    {"char",                    no_argument,        NULL,   'c'},
    {"index",                   required_argument,  NULL,   'i'},
    {"mode",                    required_argument,  NULL,   'm'},
    {"print-count-one-line",    required_argument,  NULL,   'P'},
    {"bin-file",                required_argument,  NULL,   'b'},
    {"help",                    no_argument,        NULL,   'h'},
    {"log-level",               required_argument,  NULL,   'd'},
    {"verbose",                 no_argument,        NULL,   'v'},
    {NULL,                      0,                  NULL,   0},
};

static __attribute__((noreturn)) void usage(const char *prog, FILE *fp, int _exit)
{
    int len_prog = strlen(prog);
    int i;

    fprintf(fp, "%s: [-f,--file file]"
                   " [-o,--offset offset]"
                   " [-w,--width width]"
                   " [-t,--step step]"
                   " [-s,--size size]"
                   " [-n,--number number]\n",
                prog);
    fprintf(fp, "%*.*s  [-c,--char]"
                      " [-i,--index index]"
                      " [-m,--mode mode]\n",
                len_prog, len_prog, "");
    fprintf(fp, "%*.*s  [-P,--print-count-one-line print_cnt_one_line]\n",
                len_prog, len_prog, "");
    fprintf(fp, "%*.*s  [-b,--bin-file bin_file]|[<data> ...]\n",
                len_prog, len_prog, "");
    fprintf(fp, "%*.*s  [-?,-h,--help]"
                      " [-d,--log-level level]"
                      " [-v,--verbose]\n",
                len_prog, len_prog, "");

    fprintf(fp, "\n");
    fprintf(fp, "Read or write file data element. "
                "Such as /dev/mem for access physical memory.\n");

    fprintf(fp, "\n");
    fprintf(fp, "OPTION:\n");
    fprintf(fp, "  -f,--file         file: File to be accessed.\n"
                "                          Default /dev/mem\n");
    fprintf(fp, "  -o,--offset     offset: File offset.\n"
                "                          Default 0.\n");
    fprintf(fp, "  -w,--width       width: Width of data elements (in bytes).\n"
                "                          Optional: 1, 2, 4 or 8.\n"
                "                          Default 1 byte(s).\n");
    fprintf(fp, "  -t,--step         step: Access by step, number of width.\n"
                "                          Default 1 (non-interval).\n");
    fprintf(fp, "  -s,--size         size: Sizeof address space.\n"
                "                          Default number * (width * step).\n");
    fprintf(fp, "  -n,--number     number: Number of data elements.\n"
                "                          Default 1.\n");
    fprintf(fp, "  -c,--char             : Print byte characters.\n");
    fprintf(fp, "  -i,--index       index: Data element index in non-interval "
                                          "(step > 1).\n"
                "                          Default 0.\n");
    fprintf(fp, "  -m,--mode         mode: Access mode.\n"
                "                          Optional: 0 - %d (RD_ONLY, WR_ONLY, RD_WR, "
                                                            "WR_RD or RD_WR_RD).\n"
                "                          Default %d (RD_ONLY).\n", MODE_NUM - 1,
                                                                     MODE_RD_ONLY);
    fprintf(fp, "  -P,--print-count-one-line\n"
                "      print_cnt_one_line: Number of data element printed in one line.\n"
                "                          Default auto.\n");
    fprintf(fp, "  -b,--bin-file bin_file: Data source when write mode.\n");
    fprintf(fp, "                    data: Data elements if no -b,--bin-file.\n");
    fprintf(fp, "  -?,-h,--help          : Display this messages.\n");
    fprintf(fp, "  -d,--log-level   level: Log print level.\n"
                "                          Optional: 0 - %d (FATAL, ERR, WARNING, "
                                                            "NOTICE, INFO or DEBUG).\n"
                "                          Default %d (WARNING).\n", LOG_LEVEL_NUM - 1,
                                                                     LOG_LEVEL_WARNING);
    fprintf(fp, "  -v,--verbose          : Verbose for debug. "
                                          "The log level set to 5 (DEBUG).\n");

    i = 0;
    fprintf(fp, "\n");
    fprintf(fp, "NOTE:\n");
    fprintf(fp, "%3d. At least one of [-s,--size] and [-n,--number] must exist.\n", ++i);
    fprintf(fp, "%3d. If [-s,--size] and [-n,--number] exist at the same time, "
                     "the mathematical relationship needs to be satisfied:\n"
                "       size >= (number - 1) * (width * step) + width * (index + 1)\n", ++i);
    fprintf(fp, "%3d. If only [-s,--size] and no [-n,--number], "
                     "[size] needs be aligned with [width].\n"
                "     If not align, [size] will be forced to "
                     "align downward with [width].\n", ++i);
    fprintf(fp, "%3d. If [mode] cover write action, [-b,--bin-file] or [data] sequence "
                     "(ONLY ONE) must be specified.\n", ++i);
    fprintf(fp, "%3d. The size of [bin_file] MUST be equal to [number * width].\n", ++i);
    fprintf(fp, "%3d. The length of [data] sequence MUST be equal to [number].\n", ++i);

    exit(_exit);
}

static int count_valid_bit(unsigned long long n)
{
    int count = sizeof(n) * 8;
    const typeof(n) mask = 1ull << (sizeof(n) * 8 - 1);

    if (!n)
        return 0;

    while (!(n & mask)) {
        n <<= 1;
        count--;
    }

    return count;
}

#define PRINT_COUNT_ONE_LINE_MAX        32
#define PRINT_COUNT_ONE_LINE_DEFAULT    16
static void dump_memb(const union multi_pointer va,
                      const unsigned long number,
                      const enum RDWR_WIDTH width,
                      const size_t step,
                      const size_t index,
                      int print_cnt_one_line,
                      const bool print_char,
                      FILE *fp)
{
    unsigned long long i;
    int j, k;
    int idx;
    char p_tmp[256];
    const unsigned long long size = number * (width * step);
    int valid_bit;
    int addr_width;
    /* By width */
    size_t base;
    /* By byte */
    unsigned long long offset;
    /* By width */
    size_t _index;

    /* Check */
    if (!va.p) {
        LOG_ERR("No va is provided\n");
        return;
    }
    if (!step) {
        LOG_ERR("step (%llu) too small, at least 1\n", (unsigned long long)step);
        return;
    }

    /* Assignment */
    if (!print_cnt_one_line) {
        print_cnt_one_line = PRINT_COUNT_ONE_LINE_DEFAULT;

        if (width > WIDTH_HALF)
            print_cnt_one_line /= 2;
        if (width > WIDTH_WORD)
            print_cnt_one_line /= 2;
    }
    if (!fp)
        fp = STDOUT ? STDOUT : stdout;

    valid_bit = count_valid_bit(size - 1);
    addr_width = valid_bit / 4;
    if (valid_bit % 4)
        addr_width++;
    if (!addr_width)
        addr_width = 1;

    LOG_INFO("addr_width %d\n", addr_width);

    for (i = 0; i < number; i += print_cnt_one_line) {
        base = i * step + index;
        offset = base * width;
        idx = 0;

        idx += snprintf(p_tmp + idx, sizeof(p_tmp) - idx, "%0*llx:", addr_width, offset);

        for (j = 0, _index = base;
             j < print_cnt_one_line && j + i < number;
             j++, _index += step) {
            switch(width) {
            case 1:
                idx += snprintf(p_tmp + idx, sizeof(p_tmp) - idx,
                                " %0*hhx", width * 2, va.p8[_index]);
                break;
            case 2:
                idx += snprintf(p_tmp + idx, sizeof(p_tmp) - idx,
                                " %0*hx", width * 2, va.p16[_index]);
                break;
            case 4:
                idx += snprintf(p_tmp + idx, sizeof(p_tmp) - idx,
                                " %0*x", width * 2, va.p32[_index]);
                break;
            case 8:
                idx += snprintf(p_tmp + idx, sizeof(p_tmp) - idx,
                                " %0*llx", width * 2,
                                (unsigned long long)va.p64[_index]);
                break;
            }
        }

        if (print_char) {
            for (;j < print_cnt_one_line; j++)
                idx += snprintf(p_tmp + idx, sizeof(p_tmp) - idx,
                                " %*.*s", width * 2, width * 2, "");

            idx += snprintf(p_tmp + idx, sizeof(p_tmp) - idx, " | ");

            for (j = 0;
                 j < print_cnt_one_line && j + i < number;
                 j++, offset += width * step) {
                for (k = 0; k < (int)width; k++)
                    idx += snprintf(p_tmp + idx, sizeof(p_tmp) - idx, "%c",
                                    isprint(*(uint8_t *)(va.p + k + offset)) ?
                                        *(uint8_t *)(va.p + k + offset) : '.');
            }
        }

        fprintf(fp, "%s\n", p_tmp);
    }
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int fd;
    unsigned long long i;
    union multi_pointer va = { .p = NULL, };
    char *end;
    const char *file = "/dev/mem";
    int file_mode = F_OK;
    const char *file_err = NULL;
    unsigned long long offset = 0;
    enum RDWR_WIDTH width = WIDTH_BYTE;
    size_t step = 1;
    unsigned long long size = 0;
    unsigned long long size_min;
    unsigned long long number = 1;
    bool print_char = false;
    size_t index = 0;
    enum RDWR_MODE mode = MODE_RD_ONLY;
    size_t print_cnt_one_line = 0;  // Zero for auto.
    const char *bin_file = NULL;
    union multi_pointer buf = { .p = NULL, };
    enum LOG_LEVEL level = LOG_LEVEL_UNKNOWN;
    int opt;

    /* parse options */

    while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        LOG_DEBUG("optind: %d\n", optind);
        switch (opt) {
        case 'f':
            file = optarg;
            break;
        case 'o':
            offset = strtoull(optarg, &end, 0);
            if (*end) {
                fprintf(stderr, "Invalid -o,--offset \"%s\"\n", optarg);
                usage(argv[0], stderr, 126);
            }
            break;
        case 'w':
            {
                unsigned long t = strtoul(optarg, &end, 0);
                if (*end) {
                    fprintf(stderr, "Invalid -o,--offset \"%s\"\n", optarg);
                    usage(argv[0], stderr, 126);
                }
                switch (t) {
                case WIDTH_BYTE:
                case WIDTH_HALF:
                case WIDTH_WORD:
                case WIDTH_DWORD:
                    break;
                default:
                    fprintf(stderr, "Invalid -o,--offset %lu\n", t);
                    usage(argv[0], stderr, 126);
                }
                width = t;
            }
            break;
        case 't':
            step = strtoul(optarg, &end, 0);
            if (*end) {
                fprintf(stderr, "Invalid -t,--step \"%s\"\n", optarg);
                usage(argv[0], stderr, 126);
            }
            break;
        case 's':
            size = strtoull(optarg, &end, 0);
            if (*end) {
                fprintf(stderr, "Invalid -s,--size \"%s\"\n", optarg);
                usage(argv[0], stderr, 126);
            }
            if (!size) {
                fprintf(stderr, "Invalid -s,--size %llu\n", size);
                usage(argv[0], stderr, 126);
            }
            break;
        case 'n':
            number = strtoull(optarg, &end, 0);
            if (*end) {
                fprintf(stderr, "Invalid -n,--number \"%s\"\n", optarg);
                usage(argv[0], stderr, 126);
            }
            if (!number) {
                fprintf(stderr, "Invalid -n,--number %llu\n", number);
                usage(argv[0], stderr, 126);
            }
            break;
        case 'c':
            print_char = true;
            break;
        case 'i':
            index = strtoul(optarg, &end, 0);
            if (*end) {
                fprintf(stderr, "Invalid -i,--index \"%s\"\n", optarg);
                usage(argv[0], stderr, 126);
            }
            break;
        case 'm':
            {
                unsigned long t = strtoul(optarg, &end, 0);
                if (*end) {
                    fprintf(stderr, "Invalid -m,--mode \"%s\"\n", optarg);
                    usage(argv[0], stderr, 126);
                }
                if (t >= MODE_NUM) {
                    fprintf(stderr, "Invalid -m,--mode %lu\n", t);
                    usage(argv[0], stderr, 126);
                }
                mode = t;
            }
            break;
        case 'P':
            print_cnt_one_line = strtoul(optarg, &end, 0);
            if (*end) {
                fprintf(stderr, "Invalid -P,--print-count-one-line \"%s\"\n", optarg);
                usage(argv[0], stderr, 126);
            }
            if (print_cnt_one_line > PRINT_COUNT_ONE_LINE_MAX) {
                fprintf(stderr, "Invalid -P,--print-count-one-line %llu\n",
                                (unsigned long long)print_cnt_one_line);
                usage(argv[0], stderr, 126);
            }
            break;
        case 'b':
            if (access(optarg, R_OK)) {
                fprintf(stderr, "Binary file %s in option -b,--bin-file is not readable\n", optarg);
                usage(argv[0], stderr, 126);
            }
            bin_file = optarg;
            break;
        case '?':
        case 'h':
            usage(argv[0], stdout, 0);
            break;
        case 'd':
            {
                unsigned long t = strtoul(optarg, &end, 0);
                if (*end) {
                    fprintf(stderr, "Invalid -d,--log-level \"%s\"\n", optarg);
                    usage(argv[0], stderr, 126);
                }
                if (t >= LOG_LEVEL_NUM) {
                    fprintf(stderr, "Invalid -d,--log-level %lu\n", t);
                    usage(argv[0], stderr, 126);
                }
                level = t;
            }
            break;
        case 'v':
            level = LOG_LEVEL_DEBUG;
            break;
        default:
            fprintf(stderr, "Unknown option '%c'(%d).\n", opt, opt);
            usage(argv[0], stderr, 126);
            break;
        }
    }

    /* Assignment */
    if (!STDERR)
        STDERR = stderr;
    if (!STDOUT)
        STDOUT = stdout;
    if (level != LOG_LEVEL_UNKNOWN)
        log_level = level;

    /* Check */
    switch (mode) {
    case MODE_RD_ONLY:
        file_mode = R_OK;
        file_err = "readable";
        break;
    case MODE_WR_ONLY:
        file_mode = W_OK;
        file_err = "writable";
        break;
    case MODE_RD_WR:
    case MODE_WR_RD:
    case MODE_RD_WR_RD:
        file_mode = R_OK | W_OK;
        file_err = "readable or writable";
        break;
    default:
        break;
    }
    if (access(file, file_mode)) {
        fprintf(stderr, "File %s is not %s\n", file, file_err);
        exit(125);
    }

    if ((size_t)index >= step) {
        fprintf(stderr, "[index] (%llu) is larger or equal [step] (%llu)\n",
                        (unsigned long long)index, (unsigned long long)step);
        usage(argv[0], stderr, 124);
    }
    if (!size)
        size = number * (width * step);
    if (size % width)
        size = size - (size % width);
    size_min = (number - 1) * (width * step) + width * (index + 1);
    if (size < size_min) {
        fprintf(stderr, "Invalid [size] (%llu), and the minimum value is %llu\n",
                        size, size_min);
        usage(argv[0], stderr, 124);
    }

    if (mode == MODE_RD_ONLY && (bin_file || argc - optind > 0)) {
        fprintf(stderr, "[-m,--mode %d] (RD_ONLY) is not compatible with "
                        "[-b,--bin-file] or [data] sequence.\n", MODE_RD_ONLY);
        usage(argv[0], stderr, 123);
    }

    if (mode != MODE_RD_ONLY) {
        if (bin_file && argc - optind > 0) {
            fprintf(stderr, "[-b,--bin-file] is not compatible with [data] sequence.\n");
            usage(argv[0], stderr, 123);
        }

        if (bin_file || argc - optind > 0) {
            buf.p = calloc(number, width);
            if (!buf.p) {
                fprintf(stderr, "%s: calloc %llu*%d = %llu\n", strerror(errno),
                                     number, width, number * width);
                exit(123);
            }
        }

        if (bin_file) {
            ssize_t rv;
            unsigned long long i;
            uint8_t t[sizeof(buf.p64)];
            int fd;
            struct stat statbuf = {};

            stat(bin_file, &statbuf);
            if ((unsigned long long)statbuf.st_size < size_min) {
                fprintf(stderr, "Binary file (%s) is too small, "
                                "and the minimum size is %llu bytes\n",
                                bin_file, size_min);
                ret = 123;
                goto free_buf;
            }

            fd = open(bin_file, O_RDONLY);
            if (fd < 0) {
                fprintf(STDERR, "%s: open %s\n", strerror(errno), bin_file);
                ret = 123;
                goto free_buf;
            }
            for (i = 0; i < number; i++) {
                rv = read(fd, t, width);
                if (rv != width) {
                    fprintf(STDERR, "%s: read %s, %llu element, rv %ld\n",
                                    strerror(errno), bin_file, i, (long)rv);
                    close(fd);
                    ret = 123;
                    goto free_buf;
                }

                switch (width) {
                case WIDTH_BYTE:
                    buf.p8[i] = *(uint8_t *)t;
                    break;
                case WIDTH_HALF:
                    buf.p16[i] = be16toh(*(uint16_t *)t);
                    break;
                case WIDTH_WORD:
                    buf.p32[i] = be32toh(*(uint32_t *)t);
                    break;
                case WIDTH_DWORD:
                    buf.p64[i] = be64toh(*(uint64_t *)t);
                    break;
                }
            }
            close(fd);
        } else if (argc - optind > 0) {
            int i;
            unsigned long long t;

            if ((unsigned long long)argc - optind < number) {
                fprintf(stderr, "The length of [data] sequence is too small, "
                                "and the minimum length is %llu\n", number);
                ret = 123;
                goto free_buf;
            }

            for (i = 0; optind + i < argc; i++) {
                t = strtoull(argv[i + optind], &end, 0);
                if (*end) {
                    fprintf(stderr, "Invalid byte sequence [%d] parameter: \"%s\"\n",
                                    i, argv[i + optind]);
                    ret = 123;
                    goto free_buf;
                }

                switch(width) {
                case WIDTH_BYTE:
                    buf.p8[i] = t;
                    break;
                case WIDTH_HALF:
                    buf.p16[i] = t;
                    break;
                case WIDTH_WORD:
                    buf.p32[i] = t;
                    break;
                case WIDTH_DWORD:
                    buf.p64[i] = t;
                    break;
                }
            }
        } else {
            fprintf(stderr, "[-b,--bin-file] and [data] sequence are not exist.\n");
            usage(argv[0], stderr, 123);
        }
    }

    /* mmap file */
    fd = open(file, O_RDWR);
    if (fd < 0) {
        fprintf(STDERR, "%s: open %s\n", strerror(errno), file);
        ret = 122;
        goto free_buf;
    }
    va.p = mmap(NULL, size_min, PROT_READ | (mode == MODE_RD_ONLY ? 0 : PROT_WRITE),
                MAP_SHARED, fd, offset);
    if (va.p == MAP_FAILED) {
        fprintf(STDERR, "%s: mmap %s offset 0x%llx, size 0x%llx\n", strerror(errno),
                             file, offset, size_min);
        close(fd);
        ret = 122;
        goto free_buf;
    }
    close(fd);

    /* 1. read.1: RD_ONLY, RD_WR or RD_WR_RD */
    if (mode == MODE_RD_ONLY ||
        mode == MODE_RD_WR ||
        mode == MODE_RD_WR_RD) {
        dump_memb(va, number, width, step, index, print_cnt_one_line, print_char, stdout);
    }

    /* 2. write: WR_ONLY, RD_WR, WR_RD or RD_WR_RD */
    if (mode == MODE_WR_ONLY ||
        mode == MODE_RD_WR ||
        mode == MODE_WR_RD ||
        mode == MODE_RD_WR_RD) {
        for (i = 0; i < number; i++) {
            switch (width) {
            case WIDTH_BYTE:
                va.p8[i * step + index] = buf.p8[i];
                break;
            case WIDTH_HALF:
                va.p16[i * step + index] = buf.p16[i];
                break;
            case WIDTH_WORD:
                va.p32[i * step + index] = buf.p32[i];
                break;
            case WIDTH_DWORD:
                va.p64[i * step + index] = buf.p64[i];
                break;
            }
        }
    }

    /* 3. read.2: WR_RD or RD_WR_RD */
    if (mode == MODE_WR_RD ||
        mode == MODE_RD_WR_RD) {
        if (mode == MODE_RD_WR_RD)
            printf("---\n");
        dump_memb(va, number, width, step, index, print_cnt_one_line, print_char, stdout);
    }

    ret = 0;

    if (!va.p && va.p != MAP_FAILED) munmap(va.p, size_min);
free_buf:
    if (buf.p) free(buf.p);
    return ret;
}
