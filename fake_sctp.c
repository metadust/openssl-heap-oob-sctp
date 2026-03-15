
#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#ifndef MSG_NOTIFICATION
#define MSG_NOTIFICATION 0x8000
#endif

typedef ssize_t (*recvmsg_fn)(int, struct msghdr *, int);

typedef int (*socket_fn)(int, int, int);
typedef int (*setsockopt_fn)(int, int, int, const void *, socklen_t);
typedef int (*getsockopt_fn)(int, int, int, void *, socklen_t *);

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

static unsigned char g_is_fake_sctp_fd[65536];

static int sent_once = 0;

int socket(int domain, int type, int protocol) {
    socket_fn real_socket = (socket_fn)dlsym(RTLD_NEXT, "socket");
    if (real_socket == NULL) {
        errno = ENOSYS;
        return -1;
    }
    if (protocol == IPPROTO_SCTP) {
        int fd = real_socket(domain, type, 0);
        if (fd >= 0 && fd < (int)sizeof(g_is_fake_sctp_fd))
            g_is_fake_sctp_fd[fd] = 1;
        return fd;
    }
    return real_socket(domain, type, protocol);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    setsockopt_fn real_setsockopt = (setsockopt_fn)dlsym(RTLD_NEXT, "setsockopt");
    if (real_setsockopt == NULL) {
        errno = ENOSYS;
        return -1;
    }
    if (level == IPPROTO_SCTP)
        return 0;
    return real_setsockopt(sockfd, level, optname, optval, optlen);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    getsockopt_fn real_getsockopt = (getsockopt_fn)dlsym(RTLD_NEXT, "getsockopt");
    if (real_getsockopt == NULL) {
        errno = ENOSYS;
        return -1;
    }
    if (level == IPPROTO_SCTP) {

        if (optval != NULL && optlen != NULL) {
            unsigned char *p = (unsigned char *)optval;
            if (*optlen < 64)
                *optlen = 64;
            memset(p, 0, *optlen);
            for (size_t offset = 0; offset < *optlen; offset += 1) {
                unsigned char val = (offset % 2 == 0) ? 0x00 : 0xC0;
                p[offset] = val;
            }
        }
        return 0;
    }
    return real_getsockopt(sockfd, level, optname, optval, optlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    if (!sent_once) {
        sent_once = 1;
        size_t n = 8;
        if (msg != NULL && msg->msg_iov != NULL && msg->msg_iovlen > 0) {
            size_t tocopy = n;
            if (tocopy > msg->msg_iov[0].iov_len)
                tocopy = msg->msg_iov[0].iov_len;
            fprintf(stderr, "[interposer] recvmsg first call: iov_base=%p iov_len=%zu tocopy=%zu flags=%d\n",
                    msg->msg_iov[0].iov_base, (size_t)msg->msg_iov[0].iov_len, tocopy, msg->msg_flags);
            memset(msg->msg_iov[0].iov_base, 'N', tocopy);
        }
        if (msg != NULL) {
            msg->msg_flags |= MSG_NOTIFICATION;
            msg->msg_flags |= MSG_EOR;
            msg->msg_controllen = 0;
        }
        return (ssize_t)n;
    }

    errno = EAGAIN;
    return -1;
}
