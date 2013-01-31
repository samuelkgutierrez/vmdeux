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
#include <assert.h>
#include <inttypes.h>
#include <time.h>

#include "redblack.h"

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

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/* opcode is given by the bits 28:31 */
#define OP_MASK  0xF0000000U

#define N_REGISTERS 8
#define MEM_MAX_INDEX ( 1 << 9 )

#define OP0  0x00000000U
#define OP1  0x10000000U
#define OP2  0x20000000U
#define OP3  0x30000000U
#define OP4  0x40000000U
#define OP5  0x50000000U
#define OP6  0x60000000U
#define OP7  0x70000000U
#define OP8  0x80000000U
#define OP9  0x90000000U
#define OP10 0xA0000000U
#define OP11 0xB0000000U
#define OP12 0xC0000000U
#define OP13 0xD0000000U
#define OP14 0xE0000000U

/* register masks */
#define RA 0x000001C0U
#define RB 0x00000038U
#define RC 0x00000007U

char *opstrs[32] = {
    "cmov",
    "aidx",
    "aupd",
    "add",
    "mul",
    "div",
    "nand",
    "halt",
    "alloc",
    "dealloc",
    "Output",
    "in",
    "loadprog",
    "loadimm",
    NULL
};

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
    uint32_t key;
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
    /* address space using a red-black tree */
    struct rbtree *as;
} vm_t;

/* ////////////////////////////////////////////////////////////////////////// */
void
out_reg(const vm_t *vm,
        int opindx,
        uint32_t w,
        uint32_t rega,
        uint32_t regb,
        uint32_t regc)
{
    out("%08x %08x %010lu %s %"PRIu32" %"PRIu32" %"PRIu32" "
        "[0x%08x] [0x%08x] [0x%08x]\n",
        vm->pc, w, (unsigned long)vm->pc, opstrs[opindx],
        rega, regb, regc, vm->mr[rega], vm->mr[regb], vm->mr[regc]);
}

/* ////////////////////////////////////////////////////////////////////////// */
static inline int
asi_construct(const vm_t *vm,
              size_t addp_len,
              asi_t **newa)
{
    asi_t *tmp = NULL;

    if (NULL == (tmp = calloc(1, sizeof(*tmp)))) {
        out("ERR @ %d\n", __LINE__);
        return ERR_OOR;
    }
    if (NULL == (tmp->addp = calloc(addp_len, vm->word_size))) {
        out("ERR @ %d\n", __LINE__);
        return ERR_OOR;
    }
    tmp->addp_len = addp_len;
    /* key not populated here */

    *newa = tmp;
    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
/* callback for asi_t compares */
static int
cmp_asi_t_cb(const void *v1,
             const void *v2)
{
    int64_t a = ((asi_t *)(v1))->key;
    int64_t b = ((asi_t *)(v2))->key;
    return (a - b);
}

/* ////////////////////////////////////////////////////////////////////////// */
static void
asi_rb_free_cb(void *p)
{
    asi_t *tmp = (asi_t *)p;

    if (NULL != tmp->addp) {
        free(tmp->addp);
        tmp->addp = NULL;
    }
    free(tmp);
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
vm_construct(vm_t **new)
{
    vm_t *tmp = NULL;

    if (NULL == new) {
        out("ERR @ %d\n", __LINE__);
        return ERR_INVLD_INPUT;
    }
    if (NULL == (tmp = calloc(1, sizeof(*tmp)))) {
        /* fail -- just bail */
        out("ERR @ %d\n", __LINE__);
        return ERR_OOR;
    }
    /* this will get filled in later */
    tmp->app_size = 0;
    tmp->word_size = sizeof(uint32_t);
    tmp->pc = 0;
    /* create the address space */
    if (NULL == (tmp->as = rbcreate(cmp_asi_t_cb))) {
        out("ERR @ %d\n", __LINE__);
        return ERR_OOR;
    }

    *new = tmp;

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
vm_destruct(vm_t *vm)
{
    if (NULL == vm) return ERR_INVLD_INPUT;
    rbdestroy(vm->as, asi_rb_free_cb);
    free(vm);
    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static inline bool
idtaken(const vm_t *vm,
        uint32_t id)
{
    if (0 == id) {
        return true;
    }
    return (NULL != rbfind(vm->as, &id));
}

/* ////////////////////////////////////////////////////////////////////////// */
static inline int
getid(const vm_t *vm,
      uint32_t *id)
{
    static uint32_t tid = 0;

    do {
        tid += 1;
    } while (idtaken(vm, tid));
    /* at this point we should have a valid id */
    *id = tid;
    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static inline int
alloc_array(vm_t *vm,
            size_t nwords,
            uint32_t *id)
{
    static int rc = SUCCESS;
    /* available id */
    static uint32_t aid = 0;
    asi_t *asi = NULL;

    /* not dealing with zero array */
    if (NULL != id) {
        if (SUCCESS != (rc = getid(vm, &aid))) {
            return rc;
        }
    }
    if (SUCCESS != (rc = asi_construct(vm, nwords, &asi))) {
        out("ERR @ %d\n", __LINE__);
        return rc;
    }
    /* not dealing with zero array */
    if (NULL != id) {
        asi->key = aid;
        *id = aid;
    }
    /* zero array gets id 0 */
    else {
        asi->key = 0;
    }
    /* now add the thing to the rbtree */
    if (NULL != rbinsert(vm->as, (void *)asi)) {
        out("ERR @ %d\n", __LINE__);
        return ERR;
    }

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static inline struct rbnode *
find_node(const vm_t *vm,
          uint32_t id)
{
    static struct rbnode *node = NULL;

    if (NULL == (node = rbfind(vm->as, &id))) {
        out("ERR @ %d\n", __LINE__);
        return NULL;
    }
    return node;
}

/* ////////////////////////////////////////////////////////////////////////// */
static inline int
dealloc_array(vm_t *vm,
              uint32_t id)
{
    static struct rbnode *target = NULL;
    asi_t *data = NULL;

    if (0 == id) {
        fprintf(stderr, "error: can't dealloc zero array\n");
        return ERR;
    }

    target = find_node(vm, id);

    if (unlikely(NULL == target)) {
        fprintf(stderr, "freeing unalloc'd array\n");
        return ERR;
    }
    data = rbdelete(vm->as, target);
    free(data->addp);
    free(data);

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static inline asi_t *
getasip(const vm_t *vm,
        uint32_t id)
{
    struct rbnode *node = NULL;

    node = find_node(vm, id);

    if (unlikely(NULL == node)) {
        return NULL;
    }
    else {
        return (asi_t *)node->data;
    }
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
doop(vm_t *vm)
{

    static uint32_t rega = 0, regb = 0, regc = 0, w = 0;
    static asi_t *z = NULL;

    z = getasip(vm, 0);

    w = z->addp[vm->pc];

    /* machine register index */
    rega = (w & RA) >> 6; /* 6:8 */
    regb = (w & RB) >> 3; /* 3:5 */
    regc = (w & RC);      /* 0:2 */

    switch (w & OP_MASK) {
        case OP0: {
            if (0x00000000U != vm->mr[regc]) {
                vm->mr[rega] = vm->mr[regb];
            }
            break;
        }
        case OP1: {
            asi_t *asi = getasip(vm, vm->mr[regb]);
            if (unlikely(NULL == asi)) {
                return ERR;
            }
            if (unlikely(vm->mr[regc] >= asi->addp_len)) {
                fprintf(stderr, "array oob @ line %d: "
                        "requested: %"PRIu32" but max is: %lu\n",
                        __LINE__, vm->mr[regc],
                        (unsigned long)asi->addp_len);
                return ERR;
            }
            vm->mr[rega] = asi->addp[vm->mr[regc]];
            break;
        }
        case OP2: {
            asi_t *asi = getasip(vm, vm->mr[rega]);
            if (unlikely(NULL == asi)) {
                return ERR;
            }
            if (unlikely(vm->mr[regb] >= asi->addp_len)) {
                fprintf(stderr, "array oob @ line %d: "
                        "requested: %"PRIu32" but max is: %lu\n",
                        __LINE__, vm->mr[regb],
                        (unsigned long)asi->addp_len);
                return ERR;
            }
            asi->addp[vm->mr[regb]] = vm->mr[regc];
            break;
        }
        case OP3: {
            vm->mr[rega] = vm->mr[regb] + vm->mr[regc];
            break;
        }
        case OP4: {
            vm->mr[rega] =  vm->mr[regb] * vm->mr[regc];
            break;
        }
        case OP5: {
            if (unlikely(0 == vm->mr[regc])) {
                fprintf(stderr, "div by 0 @ %d\n", __LINE__);
                return ERR;
            }
            vm->mr[rega] = vm->mr[regb] / vm->mr[regc];
            break;
        }
        case OP6: {
            /* 11111111111111111111111111110101
             * 11111111111111111111111111110100
             * --------------------------------
             * 00000000000000000000000000001011 */
            vm->mr[rega] =  ~(vm->mr[regb] & vm->mr[regc]);
            break;
        }
        case OP7:
            return HALT;
        case OP8: {
            static uint32_t id = 0;
            if (SUCCESS != alloc_array(vm, vm->mr[regc], &id)) {
                return ERR;
            }
            vm->mr[regb] = id;
            break;
        }
        case OP9: {
            if (SUCCESS != dealloc_array(vm, vm->mr[regc])) {
                fprintf(stderr, "dealloc array failure @ %d\n", __LINE__);
                return ERR;
            }
            break;
        }
        case OP10: {
            printf("%c", (char)vm->mr[regc]);
            break;
        }
        case OP11: {
            static char val = 0;
            static int n = 0;
            n = scanf("%c", &val);
            if (1 != n) {
                return ERR;
            }
            if (EOF == val) {
                vm->mr[regc] = 0xFFFFFFFF;
            }
            else {
                vm->mr[regc] = val;
            }
            break;
        }
        case OP12: {
            if (0 != vm->mr[regb]) {
                asi_t *za = NULL;
                asi_t *newp = NULL;

                if (NULL == (newp = getasip(vm, vm->mr[regb]))) {
                    out("ERR @ %d\n", __LINE__);
                    return ERR;
                }
                if (NULL == (za = getasip(vm, 0))) {
                    out("ERR @ %d\n", __LINE__);
                    return ERR;
                }
                free(za->addp);
                za->addp = calloc(newp->addp_len, vm->word_size);
                za->addp_len = newp->addp_len;
                (void)memmove(za->addp, newp->addp,
                              newp->addp_len * vm->word_size);
            }
            /* else we are dealing with the current zero array */
            vm->pc = vm->mr[regc];
            return SUCCESS;
        }
        case OP13: {
            /* this op is special */
            uint32_t val = w & 0x01FFFFFFU;
            uint32_t a = ((w & 0x0E000000U) >> 25);
            /*
            00000000000000000000000000000000
                   1111111111111111111111111 - value
                1110000000000000000000000000 - reg id
            11110000000000000000000000000000 - op
            */
            vm->mr[a] = val;
            break;
        }
        default:
            fprintf(stderr, "invalid op @ %d\n", __LINE__);
            return ERR_IOOB;
    }
    vm->pc++;
    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
get_file_size(const char *path,
              size_t *size)
{
    struct stat sbuf;
    if (0 != stat(path, &sbuf)) {
        int err = errno;
        fprintf(stderr, "stat failure: %d (%s)\n", err, strerror(err));
        return ERR_IO;
    }

    *size = (size_t)sbuf.st_size;

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
    asi_t *zap = NULL;
    size_t fsize = 0;

    if (NULL == vm || NULL == exe) return ERR_INVLD_INPUT;

    if (SUCCESS != (rc = get_file_size(exe, &fsize))) {
        return rc;
    }
    if (SUCCESS != (rc = alloc_array(vm, fsize / vm->word_size, NULL))) {
        return rc;
    }
    if (NULL == (zap = getasip(vm, 0))) {
        return ERR;
    }
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
        zap->addp[word_index++] = htonl(ibuf);
    }
    /* finish setting up vm state */
    vm->app_size = word_index * vm->word_size;
    /* sanity */
    if (vm->app_size != fsize) {
        fprintf(stderr, "read inconsistency: expected: %lu read: %lu\n",
                (unsigned long)fsize, vm->app_size);
        return ERR;
    }

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
        if (unlikely(SUCCESS != rc)) {
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
    vm_destruct(vm);
    return rc;
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
