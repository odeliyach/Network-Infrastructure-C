#define _POSIX_C_SOURCE 200809
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <file>\n", argv[0]);
        exit(1);
    }

    int fd = open(argv[3], O_RDONLY);
    if (fd < 0) /*detect errors while opening the file and reading it*/
    {
        perror("ERROR: failed to open file");
        exit(1);
    }
    struct stat st;
    if (fstat(fd, &st) < 0)/*detect errors while opening the file and reading it*/
     {
        perror("ERROR: failed to get file size");
        exit(1);
    }

    uint32_t file_size = (uint32_t)st.st_size;
    uint32_t net_size = htonl(file_size);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
         perror("ERROR: socket creation failed");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)atoi(argv[2]));

    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr); /*assumed to be a valid IP address as per instructions*/
    /* בהנחיות נאמר: "Use the inet_pton() function for converting a string containing an IP address to binary representation." */


    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: failed to connect to server");
        exit(1);
    }

    if (write(sockfd, &net_size, sizeof(net_size)) != sizeof(net_size)) {
        perror("ERROR: failed to send file size");
        exit(1);
    }

    char buf[BUF_SIZE];
    ssize_t r;
    while ((r = read(fd, buf, BUF_SIZE)) > 0) {
        ssize_t sent = 0;
        while (sent < r) {
            ssize_t w = write(sockfd, buf + sent, r - sent);
            if (w < 0) {
                perror("ERROR: failed to send file data to server");
                exit(1);
            }
            sent += w;
        }
    }

    if (r < 0) {
        perror("ERROR: failed to read from file");
        exit(1);
    }
    uint32_t net_count;
     /* קבלת מספר התווים המודפסים מהשרת */
    if (read(sockfd, &net_count, sizeof(net_count)) != sizeof(net_count)) {
        perror("ERROR: failed to receive response from server");
        exit(1);
    }

    printf("# of printable characters: %u\n", ntohl(net_count));
    return 0;
}
