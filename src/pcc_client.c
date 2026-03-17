#define _POSIX_C_SOURCE 200809
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_SIZE 1024

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
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <file>\n", argv[0]);
        exit(1);
    }

    int fd = open(argv[3], O_RDONLY);
    if (fd < 0) {
        perror("failed to open file");
        exit(1);
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("failed to get file size");
        close(fd);
        exit(1);
    }

    if (st.st_size > UINT32_MAX) {
        fprintf(stderr, "File too large for protocol limits\n");
        close(fd);
        exit(1);
    }

    uint32_t file_size = (uint32_t)st.st_size;
    uint32_t net_size = htonl(file_size);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        close(fd);
        exit(1);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server IP address\n");
        close(fd);
        close(sockfd);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("failed to connect to server");
        close(fd);
        close(sockfd);
        exit(1);
    }

    if (write_exact(sockfd, &net_size, sizeof(net_size)) != 0) {
        perror("failed to send file size");
        close(fd);
        close(sockfd);
        exit(1);
    }

    char buf[BUF_SIZE];
    while (1) {
        ssize_t r = read(fd, buf, BUF_SIZE);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("failed to read from file");
            close(fd);
            close(sockfd);
            exit(1);
        }
        if (r == 0) {
            break;
        }

        if (write_exact(sockfd, buf, (size_t)r) != 0) {
            perror("failed to send file data to server");
            close(fd);
            close(sockfd);
            exit(1);
        }
    }
    uint32_t net_count;
    if (read_exact(sockfd, &net_count, sizeof(net_count)) != (ssize_t)sizeof(net_count)) {
        perror("failed to receive response from server");
        close(fd);
        close(sockfd);
        exit(1);
    }

    close(fd);
    close(sockfd);

    printf("# of printable characters: %u\n", ntohl(net_count));
    return 0;
}

// AI chat: https://github.com/features/copilot
