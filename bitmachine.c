/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <errno.h>

/* word layout */
/* OP???????????RRRRRR */
/* 0000 0000 0000 0000 */

#define PACKAGE     "bitmachine"
#define PACKAGE_VER "0.1"

#define N_REGISTERS 8

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

#define ERR_AT       __FILE__ ": "TOSTRING(__LINE__)""
#define ERR_PREFIX   "-["PACKAGE" ERROR: "ERR_AT"]- "
#define WARN_PREFIX  "-["PACKAGE" WARNING]- "

#define OOR_COMPLAIN()                                                         \
do {                                                                           \
    fprintf(stderr, ERR_PREFIX "out of resources\n");                          \
    fflush(stderr);                                                            \
} while (0)

#ifdef NDEBUG
#define out(pfa...)                                                            \
do {                                                                           \
    ;                                                                          \
} while (0)
#else
#define out(pfa...)                                                            \
do {                                                                           \
    fprintf(stdout, pfa);                                                      \
} while (0)
#endif

/* opcode is given by the bits 28:31 */
#define OP_MASK  0xF0000000
#define REG_MASK 0x000001FF

#define MEM_MAX_INDEX ( 1 << 9 )
#define WORD_MAX_INDEX 4096

#define OP0  0x00000000
#define OP1  0x10000000
#define OP2  0x20000000
#define OP3  0x30000000
#define OP4  0x40000000
#define OP5  0x50000000
#define OP6  0x60000000
#define OP7  0x70000000
#define OP8  0x80000000
#define OP9  0x90000000
#define OP10 0xA0000000
#define OP11 0xB0000000
#define OP12 0xC0000000
#define OP13 0xD0000000
#define OP14 0xE0000000

char *opstrs[32] = {
    "Conditional Move",
    "Array Index",
    "Array Update",
    "Addition",
    "Multiplication",
    "Division",
    "Nand",
    "Halt",
    "Allocation",
    "Deallocation",
    "Output",
    "Input",
    "Load Program",
    "Load Immediate",
    NULL
};

/* register masks */
#define RA 0x00000007
#define RB 0x00000038
#define RC 0x000001C0

enum {
    SUCCESS = 0,
    ERR,
    ERR_OOR,
    ERR_IO,
    ERR_IOOB,
    ERR_INVLD_INPUT
};

typedef struct vm_t {
    /* size of application image */
    size_t app_size;
    /* sizeof machine word */
    size_t word_size;
    /* machine registers */
    uint32_t mr[N_REGISTERS];
    /* program counter */
    uint32_t pc;
    uint32_t *zero_array;
    uint32_t **words;
    uint32_t *memory;
    size_t max_index;
} vm_t;


/* ////////////////////////////////////////////////////////////////////////// */
static int
vm_construct(vm_t **new)
{
    vm_t *tmp = NULL;
    int rc = SUCCESS;

    if (NULL == new) {
        rc = ERR_INVLD_INPUT;
        goto out;
    }
    if (NULL == (tmp = calloc(4096, sizeof(*tmp)))) {
        rc = ERR_OOR;
        goto out;
    }
    /* XXX update max to grow */
    if (NULL == (tmp->words = calloc(WORD_MAX_INDEX, sizeof(*tmp->words)))) {
        rc = ERR_OOR;
        goto out;
    }
    if (NULL == (tmp->memory = calloc(MEM_MAX_INDEX, sizeof(*tmp->memory)))) {
        rc = ERR_OOR;
        goto out;
    }
    tmp->app_size = 0;
    tmp->max_index = WORD_MAX_INDEX;
    tmp->word_size = sizeof(uint32_t);
    tmp->pc = 0;

    /* XXX cleanup */

out:
    /* failure */
    if (SUCCESS != rc) {
        if (tmp) {
        }
    }
    *new = tmp;
    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
vm_destruct(vm_t *vm)
{
    if (NULL == vm) return ERR_INVLD_INPUT;
    /* XXX TODO */
    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
int
doop(vm_t *vm, uint32_t w)
{
    size_t rega = (w & RA);
    size_t regb = (w & RB);
    size_t regc = (w & RC);

    switch (w & OP_MASK) {
        case OP0:
            out("(%08x) OP: %s\n", w, opstrs[0]);
            if (0 == vm->memory[regc]) {
                vm->memory[rega] = vm->memory[regb];
            }
            break;
        case OP1:
            out("(%08x) OP: %s\n", w, opstrs[1]);
            /* ??? */
            break;
        case OP2:
            out("(%08x) OP: %s\n", w, opstrs[2]);
            /* ??? */
            break;
        case OP3:
            out("(%08x) OP: %s\n", w, opstrs[3]);
            break;
        case OP4:
            out("(%08x) OP: %s\n", w, opstrs[4]);
            break;
        case OP5:
            out("(%08x) OP: %s\n", w, opstrs[5]);
            break;
        case OP6:
            out("(%08x) OP: %s\n", w, opstrs[6]);
            break;
        case OP7:
            out("(%08x) OP: %s\n", w, opstrs[7]);
            break;
        case OP8:
            out("(%08x) OP: %s\n", w, opstrs[8]);
            break;
        case OP9:
            out("(%08x) OP: %s\n", w, opstrs[9]);
            break;
        case OP10: {
            uint8_t out = vm->mr[regc] % 256;
            printf("%c", out);
            break;
        }
        case OP11:
            out("(%08x) OP: %s\n", w, opstrs[11]);
            break;
        case OP12:
            out("(%08x) OP: %s\n", w, opstrs[12]);
            break;
        case OP13: {
            /* this op is special */
            uint32_t val = w & 0x000000FFFFFF;
            /* 0x000007000000 describes the target register id */
            vm->mr[w & 0x000007000000] = val;
            break;
        }
        default:
            return ERR_IOOB;
    }

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
/* TODO rename and update to provide simple disassembler functionality */
int
echo_app(vm_t *vm)
{
    size_t size = vm->app_size / vm->word_size;
    size_t i;
    out("o app size: %lu\n", (unsigned long)vm->app_size);
    out("o max word index: %lu\n", (unsigned long)vm->max_index);

    for (i = 0; i < size; ++i) {
        printf("(%08x): OP: %08x REG: %08x\n",
               vm->words[0][i],
               vm->words[0][i] & OP_MASK,
               vm->words[0][i] & REG_MASK);
        doop(vm, vm->zero_array[i]);
    }

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
load_app(vm_t *vm, const char *exe)
{
    int fd = -1;
    ssize_t bread = 0;
    uint32_t ibuf = 0;
    int rc = SUCCESS;
    size_t word_index = 0;

    if (NULL == vm || NULL == exe) return ERR_INVLD_INPUT;

    out("o reading: %s\n", exe);

    if (-1 == (fd = open(exe, O_RDONLY))) {
        int err = errno;
        fprintf(stderr, "open failure: %d (%s)\n", err, strerror(err));
        return ERR_IO;
    }
    while ((bread = read(fd, &ibuf, sizeof(ibuf)))) {
        if (-1 == bread) {
            int err = errno;
            fprintf(stderr, "read failure: %d (%s)\n", err, strerror(err));
            rc = ERR_IO;
            goto out;
        }
        else if (sizeof(ibuf) != bread) {
            fprintf(stderr, "read inconsistency: read: %lu expected: %lu\n",
                    (unsigned long)bread, (unsigned long)sizeof(ibuf));
            rc = ERR_IO;
            goto out;
        }
        /* else all is well, so append word to program "0" array */
        /* NOTE: a small over allocation... just by one (so no malloc 0) */
        vm->words[0] = realloc(vm->words[0],
                               (word_index + 1) * vm->word_size);
        vm->words[0][word_index++] = htonl(ibuf);
        vm->zero_array = vm->words[0];
    }
    vm->app_size = word_index * vm->word_size;

out:
    if (-1 != fd) {
        close(fd);
    }
    return rc;
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
run(vm_t *vm)
{
    int i;
    for (i = 0; i < vm->app_size / vm->word_size; ++i) {
        doop(vm, vm->zero_array[i]);
    }
    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
go(const char *exe)
{
    int rc = SUCCESS;
    vm_t *vm = NULL;

    if (SUCCESS != (rc = vm_construct(&vm))) {
        fprintf(stderr, "vm_construct error: %d\n", rc);
        return rc;
    }
    if (SUCCESS != (rc = load_app(vm, exe))) {
        fprintf(stderr, "load_app error: %d\n", rc);
        /* rc is set */
        goto out;
    }
    if (SUCCESS != (rc = run(vm))) {
        fprintf(stderr, "run error: %d\n", rc);
        goto out;
    }

out:
    return vm_destruct(vm);
}

/* ////////////////////////////////////////////////////////////////////////// */
static void
usage(void)
{
    printf("usage: %s APP\n", PACKAGE);
}

/* ////////////////////////////////////////////////////////////////////////// */
/* ////////////////////////////////////////////////////////////////////////// */
/* ////////////////////////////////////////////////////////////////////////// */
int
main(int argc, char **argv)
{
    int rc = ERR;

    /* enough args? */
    if (2 != argc) {
        usage();
        return EXIT_FAILURE;
    }
    /* valid path? */
    else if (-1 == access(argv[1], F_OK | R_OK)) {
        int err = errno;
        fprintf(stderr, "cannot read %s - %s.\n", argv[1], strerror(err));
        usage();
        return EXIT_FAILURE;
    }
    /* if we are here, then we can read the input file */
    if (SUCCESS != (rc = go(argv[1]))) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
