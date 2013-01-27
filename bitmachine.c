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

#define PACKAGE     "bitmachine"
#define PACKAGE_VER "0.1"

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

#define N_REGISTERS 8
#define MEM_MAX_INDEX ( 1 << 9 )
#define AS_ARRAY_SIZE ( 1 << 16 )

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
#define RA 0x000001C0
#define RB 0x00000038
#define RC 0x00000007

enum {
    SUCCESS = 0,
    ERR,
    ERR_OOR,
    ERR_IO,
    ERR_IOOB,
    ERR_INVLD_INPUT,
    HALT
};

/* address space item typedef'd stuct */
typedef struct asi_t {
    bool used;
    uint32_t *addp;
    size_t addp_len;
} asi_t;

typedef struct vm_t {
    /* size of application image */
    size_t app_size;
    /* sizeof machine word */
    size_t word_size;
    /* machine registers */
    uint32_t mr[N_REGISTERS];
    /* program counter */
    uint32_t pc;
    /* points to zero array */
    uint32_t *zero_array;
    /*address space */
    asi_t **addr_space;
} vm_t;


/* ////////////////////////////////////////////////////////////////////////// */
static int
vm_construct(vm_t **new)
{
    vm_t *tmp = NULL;

    if (NULL == new) {
        return ERR_INVLD_INPUT;
    }
    if (NULL == (tmp = calloc(1, sizeof(*tmp)))) {
        /* fail -- just bail */
        return ERR_OOR;
    }
    /* this will get filled in later */
    tmp->app_size = 0;
    tmp->word_size = sizeof(uint32_t);
    tmp->pc = 0;
    tmp->zero_array = NULL;
    tmp->addr_space = calloc(AS_ARRAY_SIZE, sizeof(asi_t *));
    tmp->addr_space[0] = calloc(AS_ARRAY_SIZE, sizeof(asi_t));
    /* mark as used because we don't want any zero array confusion madness */
    tmp->addr_space[0][0].used = true;

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
static uint32_t
find_avail_id(vm_t *vm)
{
    int i, j;

    for (i = 0; i < AS_ARRAY_SIZE; ++i) {
        for (j = 0; j < AS_ARRAY_SIZE; ++j) {
            /* we are on to a new array */
            if (NULL == vm->addr_space[i]) {
                out("           %s: creating new asi_t array @ %d\n", __func__,
                    i);
                vm->addr_space[i] = calloc(AS_ARRAY_SIZE, sizeof(asi_t));
            }
            if (!vm->addr_space[i][j].used) {
                vm->addr_space[i][j].used = true;
                out("           %s: found unused spot @ %d %d returned %d\n",
                    __func__, i, j, (i * AS_ARRAY_SIZE) + j);
                return (i * AS_ARRAY_SIZE) + j;
            }
        }
    }
    /* XXX FIXME */
    return -1;
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
alloc_array(vm_t *vm,
            size_t nwords,
            uint32_t *id)
{
    /* XXX make sure we have a used id from find_avail_id */
    *id = find_avail_id(vm);
    int i = (*id / AS_ARRAY_SIZE), j = (*id % AS_ARRAY_SIZE);
    vm->addr_space[i][j].addp = calloc(nwords, vm->word_size);
    vm->addr_space[i][j].addp_len = nwords;

    out("           %s: id %lu allocating %lu words @ [%d, %d]\n",
        __func__, (unsigned long)*id, (unsigned long)nwords, i, j);

    return SUCCESS;

}

/* ////////////////////////////////////////////////////////////////////////// */
static int
get_array(vm_t *vm,
          uint32_t id,
          int *i,
          int *j)
{
    *i = (id / AS_ARRAY_SIZE);
    *j = (id % AS_ARRAY_SIZE);

    out("           %s: id %lu ==> [%d, %d]\n", __func__,
        (unsigned long)id, *i, *j);

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
dealloc_array(vm_t *vm,
              uint32_t id)
{
    int i = id / AS_ARRAY_SIZE, j = id % AS_ARRAY_SIZE;
    free(vm->addr_space[i][j].addp);
    vm->addr_space[i][j].addp = NULL;
    vm->addr_space[i][j].used = false;
    vm->addr_space[i][j].addp_len = 0;

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
int
doop(vm_t *vm)
{
    uint32_t w = vm->zero_array[vm->pc];

    /* machine register index */
    size_t rega = (w & RA) >> 6; /* 6:8 */
    size_t regb = (w & RB) >> 3; /* 3:5 */
    size_t regc = (w & RC);      /* 0:2 */

    switch (w & OP_MASK) {
        case OP0: {
            if (0 != vm->mr[regc]) {
                out("[%08x @ %lu] %s: reg %lu != 0, so setting reg %lu to %lu\n", w,
                    (unsigned long)vm->pc, opstrs[0], (unsigned long)regc,
                    (unsigned long)rega, (unsigned long)vm->mr[regb]);
                vm->mr[rega] = vm->mr[regb];
            }
            else {
                out("[%08x @ %lu] %s: reg %lu == 0, so doing nothing\n", w,
                    (unsigned long)vm->pc, opstrs[0], (unsigned long)regc);
            }
            break;
        }
        case OP1: {
            int i, j;
            get_array(vm, vm->mr[regb], &i, &j);
            /* XXX check bounds */
            vm->mr[rega] = vm->addr_space[i][j].addp[vm->mr[regc]];
            out("[%08x @ %lu] %s: MR @ %d = [%d][%d] @ OFFSET %lu\n", w,
                (unsigned long)vm->pc, opstrs[1], (int)regb, i, j,
                (unsigned long)vm->mr[regc]);
            break;
        }
        case OP2: {
            int i, j;
            get_array(vm, vm->mr[rega], &i, &j);
            vm->addr_space[i][j].addp[vm->mr[regb]] = vm->mr[regc];
            out("[%08x @ %lu] %s: [%d][%d] @ OFFSET %lu = %lu\n", w, (unsigned long)vm->pc,
                opstrs[2], i, j, (unsigned long)vm->mr[regb],
                (unsigned long)vm->mr[regc]);
            break;
        }
        case OP3: {
            uint64_t a = vm->mr[rega], b = vm->mr[regb], c = vm->mr[regc];

            a = (b + c) % 4294967296;
            vm->mr[rega] = (uint32_t)a;
            printf("[%08x @ %lu] %s: %llu = (%llu + %llu)\n", w, (unsigned long)vm->pc,
                opstrs[3], (unsigned long long)vm->mr[rega],
                (unsigned long long)b,
                (unsigned long long)c);
            break;
        }
        case OP4: {
            uint64_t a = vm->mr[rega], b = vm->mr[regb], c = vm->mr[regc];

            a = (b  * c) % 4294967296;
            vm->mr[rega] = (uint32_t)a;
            printf("[%08x @ %lu] %s: %lu = (%lu * %lu)\n", w, (unsigned long)vm->pc,
                opstrs[4], (unsigned long)vm->mr[rega],
                (unsigned long)b,
                (unsigned long)c);
            break;
        }
        case OP5: {
            vm->mr[rega] = (vm->mr[regb] / vm->mr[regc]);
            out("[%08x @ %lu] %s: %lu = (%lu / %lu)\n", w, (unsigned long)vm->pc,
                opstrs[5], (unsigned long)vm->mr[rega], (unsigned long)vm->mr[regb],
                (unsigned long)vm->mr[regc]);
            break;
        }
        case OP6: {
            /* 11111111111111111111111111110101
             * 11111111111111111111111111110101
             * --------------------------------
             * 00000000000000000000000000001010 */
            uint32_t a = vm->mr[rega], b  = vm->mr[regb], c = vm->mr[regc];
            a = ~(b & c);

            vm->mr[rega] = a;
            out("[%08x @ %lu] %s: %08x = (%08x NAND %08x)\n", w, (unsigned long)vm->pc,
                opstrs[6], a, b, c);
            break;
        }
        case OP7:
            out("[%08x @ %lu] %s:\n", w, (unsigned long)vm->pc, opstrs[7]);
            return HALT;
        case OP8: {
            uint32_t id = 0;
            if (SUCCESS != alloc_array(vm, vm->mr[regc], &id)) {
                return ERR;
            }
            printf("############## %lu\n", vm->mr[regb]);
            vm->mr[regb] = id;
            out("[%08x @ %lu] %s: alloc'd %lu words setting reg %d to %lu\n", w,
                (unsigned long)vm->pc, opstrs[8], (unsigned long)vm->mr[regc],
                (int)vm->mr[regb], (unsigned long)id);
            break;
        }
        case OP9: {
            if (SUCCESS != dealloc_array(vm, vm->mr[regc])) {
                return ERR;
            }
            out("[%08x @ %lu] %s: %lu\n", w, (unsigned long)vm->pc,
                opstrs[9], (unsigned long)vm->mr[regc]);
            break;
        }
        case OP10: {
            uint8_t out = vm->mr[regc] % 256;
            printf("%c", out);
            break;
        }
        case OP11: {
            char inbuf[128];
            int val = 0;
            /* XXX error checks */ 
            printf("           w: %08x\n", w);
            fgets(inbuf, sizeof(inbuf), stdin);
            printf("           input: %s\n", inbuf);
            val = (int)strtoul(inbuf, NULL, 10);
            if (val > 256) {
                return ERR;
            }
            printf("VAL: %d\n", val);
            vm->mr[regc] = val;
            printf("REGC: %08x\n", vm->mr[regc]);
            w |= 0x00000007;
            printf("           w: %08x\n", w);
            break;
        }
        case OP12: {
            int i = 0, j = 0;

            /* if we are given the zero array, there is nothing to do */
            if (0 != vm->mr[regb]) {
                uint32_t *tmp_zarray = NULL;
                size_t len = 0, k = 0;
                get_array(vm, vm->mr[regb], &i, &j);
                len = vm->addr_space[i][j].addp_len;
                tmp_zarray = calloc(len, sizeof(uint32_t));
                for (k = 0; k < len; ++k) {
                    tmp_zarray[k] = vm->addr_space[i][j].addp[k];
                }
                free(vm->zero_array);
                vm->zero_array = tmp_zarray;
            }
            /* else we are dealing with the current zero array */
            out("[%08x @ %lu] %s: dup'ing %lu [%d, %d] "
                "and replacing zero array. pc set to %lu\n",
                w, (unsigned long)vm->pc, opstrs[12],
                (unsigned long)vm->mr[regb], i, j, (unsigned long)vm->mr[regc]);
            vm->pc = vm->mr[regc] - 1;
            break;
        }
        case OP13: {
            /* this op is special */
            uint32_t val = w & 0x01FFFFFF;
            uint32_t index = ((w & 0x0E000000) >> 25);
            /*
            00000000000000000000000000000000
                   1111111111111111111111111 - value
                1110000000000000000000000000 - reg id
            11110000000000000000000000000000 - op
            */
            vm->mr[index] = val;
            out("[%08x @ %lu] %s: setting reg %lu to %lu\n", w,
                (unsigned long)vm->pc, opstrs[13],
                (unsigned long)index, (unsigned long)val);
            break;
        }
        default:
            out("INVALID OP!\n");
            return ERR_IOOB;
    }
    vm->pc++;
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
        vm->zero_array = realloc(vm->zero_array,
                                 (word_index + 1) * vm->word_size);
        vm->zero_array[word_index++] = htonl(ibuf);
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
    int rc;

    while (true) {
        rc = doop(vm);
        if (SUCCESS != rc) {
            if (HALT == rc) {
                rc = SUCCESS;
            }
            break;
        }
    }
    return rc;
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
