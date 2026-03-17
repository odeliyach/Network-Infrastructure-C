#define _POSIX_C_SOURCE 200809
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024 /*Do not assume that you can allocate a buffer whose size is equal to the file size*/
#define FIRST_PRINT 32
#define LAST_PRINT 126

static volatile sig_atomic_t sigint_flag = 0;

static uint32_t pcc_total[LAST_PRINT + 1] = {0};
static uint32_t served_clients = 0;

void sigint_handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2)/*You should validate that the correct number of command line arguments is passed.*/
     {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    if (sigaction(SIGINT, &sa, NULL) != 0) 
    {
    perror("ERROR: failed to set SIGINT handler");
    exit(1);
    }

    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(serv_addr);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("ERROR: socket creation failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons((uint16_t)atoi(argv[1]));
    /*Use bind() to the address INADDR_AN*/
    if (bind(listenfd, (struct sockaddr *)&serv_addr, addrsize) != 0) 
    {
         perror("ERROR: bind failed");
        exit(1);
    }

    if (listen(listenfd, 10) != 0) {
        perror("ERROR: listen failed");
        exit(1);
    }

    while (!sigint_flag) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            if (errno == EINTR)
                continue;
            perror("ERROR: accept failed");
            exit(1);
        }

        uint32_t net_n;
        if (read(connfd, &net_n, sizeof(net_n)) != sizeof(net_n)) {
            perror("ERROR: failed to read file size from client");
            close(connfd);
            continue;
        }

        uint32_t n = ntohl(net_n);
        uint32_t local_hist[LAST_PRINT + 1] = {0};
        uint32_t printable_count = 0;

        char buf[BUF_SIZE];
        uint32_t remaining = n;

        while (remaining > 0) {
            ssize_t to_read = remaining < BUF_SIZE ? remaining : BUF_SIZE;
            ssize_t r = read(connfd, buf, to_read);
            if (r == 0) /*Do not exit the server. Just print an error message to stderr and keep accepting new client connections.*/
            { /* הלקוח סגר את החיבור */
                fprintf(stderr, "Client closed connection unexpectedly\n");
                close(connfd);
                goto next_client;
            } else if (r < 0) {
               if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
                    fprintf(stderr, "TCP error on client connection: %s\n", strerror(errno));
                    close(connfd);
                    goto next_client; /* לא מעדכנים את הסטטיסטיקה */
                } else {
                    perror("read");
                    close(connfd);
                    exit(1);
                }
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
        if (write(connfd, &net_c, sizeof(net_c)) != sizeof(net_c)) {
            if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
                fprintf(stderr, "TCP error on client connection while writing: %s\n", strerror(errno));
                close(connfd);
                goto next_client;
            } else {
                perror("write");
                close(connfd);
                exit(1);
            }
        }

        for (int i = FIRST_PRINT; i <= LAST_PRINT; i++)
            pcc_total[i] += local_hist[i];

        served_clients++;
        close(connfd);

    next_client:
        if (sigint_flag)
            break;
    }

    for (int i = FIRST_PRINT; i <= LAST_PRINT; i++) {
        if (pcc_total[i] > 0)
            printf("char '%c' : %u times\n", i, pcc_total[i]);
    }

    printf("Served %u client(s) successfully\n", served_clients);
    return 0;
}