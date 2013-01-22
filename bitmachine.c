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

/* ////////////////////////////////////////////////////////////////////////// */
/* TODO
 * input validation
 * */

/* ////////////////////////////////////////////////////////////////////////// */


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

enum {
    SUCCESS = 0,
    ERR,
    ERR_OOR,
    ERR_IO
};

/* ////////////////////////////////////////////////////////////////////////// */
int
go(const char *what)
{
    int fd = -1;
    ssize_t bread = 0;
    uint32_t ibuf = 0;
    int rc = SUCCESS;

    fprintf(stdout, "reading: %s\n", what);

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
        printf("%08x\n", htonl(ibuf));
    }

out:
    if (-1 != fd) {
        close(fd);
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


out:
    return erc;
}
