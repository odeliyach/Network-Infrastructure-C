#define _POSIX_C_SOURCE 200809
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define FIRST_PRINT 32
#define LAST_PRINT 126

static volatile sig_atomic_t sigint_flag = 0;

static uint32_t pcc_total[LAST_PRINT + 1] = {0};
static uint32_t served_clients = 0;

void sigint_handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

static bool is_tcp_disconnect_error(void) {
    return errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE;
}

static ssize_t read_exact(int fd, void *buf, size_t len) {
    size_t offset = 0;
    while (offset < len) {
        ssize_t r = read(fd, (char *)buf + offset, len - offset);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (r == 0) {
            return (ssize_t)offset;
        }
        offset += (size_t)r;
    }
    return (ssize_t)offset;
}

static int write_exact(int fd, const void *buf, size_t len) {
    size_t offset = 0;
    while (offset < len) {
        ssize_t w = write(fd, (const char *)buf + offset, len - offset);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (w == 0) {
            continue;
        }
        offset += (size_t)w;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) != 0) {
        perror("failed to set SIGINT handler");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(serv_addr);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket creation failed");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(listenfd);
        exit(1);
    }

    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons((uint16_t)atoi(argv[1]));
    if (bind(listenfd, (struct sockaddr *)&serv_addr, addrsize) != 0) {
        perror("bind failed");
        close(listenfd);
        exit(1);
    }

    if (listen(listenfd, 10) != 0) {
        perror("listen failed");
        close(listenfd);
        exit(1);
    }

    while (!sigint_flag) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept failed");
            close(listenfd);
            exit(1);
        }

        uint32_t net_n;
        ssize_t header_bytes = read_exact(connfd, &net_n, sizeof(net_n));
        if (header_bytes != (ssize_t)sizeof(net_n)) {
            if (header_bytes < 0 && is_tcp_disconnect_error()) {
                fprintf(stderr, "TCP error while reading client payload size: %s\n", strerror(errno));
                close(connfd);
                goto next_client;
            }
            if (header_bytes < 0) {
                perror("failed to read file size from client");
                close(connfd);
                exit(1);
            }
            fprintf(stderr, "Client closed connection before sending payload size\n");
            close(connfd);
            goto next_client;
        }

        uint32_t n = ntohl(net_n);
        uint32_t local_hist[LAST_PRINT + 1] = {0};
        uint32_t printable_count = 0;

        char buf[BUF_SIZE];
        uint32_t remaining = n;

        while (remaining > 0) {
            ssize_t to_read = remaining < BUF_SIZE ? remaining : BUF_SIZE;
            ssize_t r = read(connfd, buf, to_read);
            if (r == 0) {
                fprintf(stderr, "Client closed connection unexpectedly\n");
                close(connfd);
                goto next_client;
            } else if (r < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (is_tcp_disconnect_error()) {
                    fprintf(stderr, "TCP error on client connection: %s\n", strerror(errno));
                    close(connfd);
                    goto next_client;
                }
                perror("read from client failed");
                close(connfd);
                exit(1);
            }

            for (ssize_t i = 0; i < r; i++) {
                unsigned char c = buf[i];
                if (c >= FIRST_PRINT && c <= LAST_PRINT) {
                    printable_count++;
                    local_hist[c]++;
                }
            }
            remaining -= r;
        }

        uint32_t net_c = htonl(printable_count);
        if (write_exact(connfd, &net_c, sizeof(net_c)) != 0) {
            if (is_tcp_disconnect_error()) {
                fprintf(stderr, "TCP error on client connection while writing: %s\n", strerror(errno));
                close(connfd);
                goto next_client;
            }
            perror("write to client failed");
            close(connfd);
            exit(1);
        }

        for (int i = FIRST_PRINT; i <= LAST_PRINT; i++)
            pcc_total[i] += local_hist[i];

        served_clients++;
        close(connfd);

    next_client:
        if (sigint_flag)
            break;
    }

    close(listenfd);

    for (int i = FIRST_PRINT; i <= LAST_PRINT; i++) {
        if (pcc_total[i] > 0)
            printf("char '%c' : %u times\n", i, pcc_total[i]);
    }

    printf("Served %u client(s) successfully\n", served_clients);
    return 0;
}

// AI chat: https://github.com/features/copilot
