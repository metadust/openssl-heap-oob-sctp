
#include <openssl/bio.h>
#include <openssl/err.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

const BIO_METHOD *BIO_s_datagram_sctp(void);
BIO *BIO_new_dgram_sctp(int fd, int close_flag);

int main(void) {
    ERR_clear_error();
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_SCTP);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    BIO *b = BIO_new_dgram_sctp(fd, BIO_NOCLOSE);
    if (b == NULL) {
        fprintf(stderr, "BIO_new_dgram_sctp failed\n");
        ERR_print_errors_fp(stderr);
        fprintf(stderr, "errno=%d (%s)\n", errno, strerror(errno));
        return 1;
    }
    unsigned char *out = (unsigned char *)malloc(1);
    if (out == NULL) {
        perror("malloc");
        return 1;
    }
    int r = BIO_read(b, (char *)out, 1);
    fprintf(stderr, "BIO_read returned %d\n", r);
    BIO_free(b);
    free(out);
    return 0;
}
