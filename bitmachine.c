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

/* ////////////////////////////////////////////////////////////////////////// */
/* TODO
 * input validation
 * */

/* ////////////////////////////////////////////////////////////////////////// */

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

#define OP_MASK 0xFF000000

#if 0
#define OP0(w)  ( ((w) & OP_MASK) == 0x00000000 )
#define OP1(w)  ( ((w) & OP_MASK) == 0x10000000 )
#define OP2(w)  ( ((w) & OP_MASK) == 0x20000000 )
#define OP3(w)  ( ((w) & OP_MASK) == 0x30000000 )
#define OP4(w)  ( ((w) & OP_MASK) == 0x40000000 )
#define OP5(w)  ( ((w) & OP_MASK) == 0x50000000 )
#define OP6(w)  ( ((w) & OP_MASK) == 0x60000000 )
#define OP7(w)  ( ((w) & OP_MASK) == 0x70000000 )
#define OP8(w)  ( ((w) & OP_MASK) == 0x80000000 )
#define OP9(w)  ( ((w) & OP_MASK) == 0x90000000 )
#define OP10(w) ( ((w) & OP_MASK) == 0xA0000000 )
#define OP11(w) ( ((w) & OP_MASK) == 0xB0000000 )
#define OP12(w) ( ((w) & OP_MASK) == 0xC0000000 )
#define OP13(w) ( ((w) & OP_MASK) == 0xD0000000 )
#define OP14(w) ( ((w) & OP_MASK) == 0xE0000000 )
#else
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
#endif

enum {
    SUCCESS = 0,
    ERR,
    ERR_OOR,
    ERR_IO,
    ERR_IOOB,
    ERR_INLD_INPUT
};

/* machine registers */
typedef struct registers_t {
    uint32_t r[N_REGISTERS];
} registers_t;

typedef struct vm_t {
    size_t app_size;
    /* machine registers */
    registers_t mr;
    uint32_t *zero_array;
    uint32_t **words;
    size_t max_index;
} vm_t;


/* ////////////////////////////////////////////////////////////////////////// */
static int
vm_construct(vm_t **new)
{
    vm_t *tmp = NULL;
    int rc = SUCCESS;

    if (NULL == new) {
        rc = ERR_INLD_INPUT;
        goto out;
    }
    if (NULL == (tmp = calloc(4096, sizeof(*tmp)))) {
        rc = ERR_OOR;
        goto out;
    }
    if (NULL == (tmp->words = calloc(4096, sizeof(*tmp->words)))) {
        rc = ERR_OOR;
        goto out;
    }
    tmp->app_size = 0;
    tmp->max_index = 4096;
    tmp->zero_array = tmp->words[0];

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

#if 0
//static int
//vm_destruct(vm_t **old)
//{
//    /* TODO */
//    return SUCCESS;
//}
#endif

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

/* ////////////////////////////////////////////////////////////////////////// */
int
doop(uint32_t w, vm_t *vm)
{
    switch (w & OP_MASK) {
        case OP0:
            out("(%08x) OP: %s\n", w, opstrs[0]);
            break;
        case OP1:
            out("(%08x) OP: %s\n", w, opstrs[1]);
            break;
        case OP13:
            out("(%08x) OP: %s\n", w, opstrs[13]);
            break;
        default:
            return ERR_IOOB;
    }

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
static int
store_app(vm_t *vm, uint32_t buf)
{
    static int index = 0;
    static bool init = true;

    if (init) {
        /* XXX cleanup -needs realloc or something */
        vm->words[0] = calloc(4096, sizeof(uint32_t));
        init = false;
    }
    vm->words[0][index] = htonl(buf);
    vm->app_size = (index++ * sizeof(uint32_t));

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
int
echo_app(vm_t *vm)
{
    size_t index = vm->app_size / sizeof(uint32_t);
    size_t i;
    out("app size: %lu\n", (unsigned long)vm->app_size);

    for (i = 0; i < index; ++i) {
        printf("%08x\n", vm->words[0][i]);
    }

    return SUCCESS;
}

/* ////////////////////////////////////////////////////////////////////////// */
int
go(const char *what)
{
    int fd = -1;
    ssize_t bread = 0;
    uint32_t ibuf = 0;
    int rc = SUCCESS;
    vm_t *vm = NULL;

    if (SUCCESS != (rc = vm_construct(&vm))) {
        fprintf(stderr, "vm_construct error: %d\n", rc);
        return rc;
    }

    out("reading: %s\n", what);

    if (-1 == (fd = open(what, O_RDONLY))) {
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
        /* else all is well */
        /* convert to big endian */
        if (SUCCESS != (rc = store_app(vm, ibuf))) {
            fprintf(stderr, "store_app failure: %d\n", rc);
            /* rc is set */
            goto out;
        }
    }

    echo_app(vm);

out:
    if (-1 != fd) {
        close(fd);
    }
    if (NULL != vm) {
        free(vm);
    }
    return rc;
}

/* ////////////////////////////////////////////////////////////////////////// */
/* ////////////////////////////////////////////////////////////////////////// */
/* ////////////////////////////////////////////////////////////////////////// */
int
main(int argc, char **argv)
{
    int erc = EXIT_FAILURE;
    int rc = ERR;

    if (SUCCESS != (rc = go(argv[1]))) {
        goto out;
    }
    erc = EXIT_SUCCESS;

out:
    return erc;
}
