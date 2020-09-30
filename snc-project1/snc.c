#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>

void error() {
    printf("internal error");
    kill(0, SIGKILL);
}

void handle_server_kill_message(int sock, int protocol) { // handle input reads for server
    char buffer[100];
    for (;;) {
        char *line = fgets(buffer, 255, stdin);
        if (line == NULL) {
            if (protocol == IPPROTO_TCP) {
                write(sock, "0x03", 18);
                close(sock);
                error();
            }
        }
        bzero(buffer, 100);
    }
}

void handle_client_read_tcp(int sock) { //handle socket reads for client
    char buffer[256];
    for (;;) {
        int n = (int) read(sock, buffer, 255);
        if (strcmp(buffer, "0x03") == 0) {
            close(sock);
            error();
        }
        if (n < 1) {
            close(sock);
            error();
        }
        if (n != 0) {
            printf("%s\n", buffer);
        }
    }
}

void handle_session_tcp(int sock) { //handler multiple server connections
    int n;
    char buffer[256];
    for (;;) {
        bzero(buffer, 256);
        n = (int) read(sock, buffer, 255);
        if (strcmp(buffer, "0x03") == 0) {
            close(sock);
            error();
        }
        if (n < 1) {
            close(sock);
            error();
        }
        if (n != 0) {
            printf("Here is the message: %s\n", buffer);
        }
        n = (int) write(sock, "I got your message", 18);
        if (n < 0) error();
    }
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno, clilen, pid, kill_pid, read_pid, addr_size;
    struct sockaddr_in serv_addr, cli_addr;
    struct hostent *server;
    int n;
    if (argc < 3 || argc > 5) {
        printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
        exit(1);
    }
    int protocol = IPPROTO_TCP;
    int sock_type = SOCK_STREAM;
    int type_flag = 0;
    for (int i = 1; i <= 2; i++) { //read flags
        if (strcmp(argv[i], "-u") == 0) {
            protocol = IPPROTO_UDP;
            sock_type = SOCK_DGRAM;
        }
        if (strcmp(argv[i], "-l") == 0) {
            type_flag = 1;
        }
    }

    server = gethostbyname(argv[argc - 2]);

    int client_flag = 0;
    char *client = "";
    if (type_flag) { //determine if server instance includes hostname
        if ((argc >= 5 && protocol == IPPROTO_UDP) || (argc >= 4 && protocol == IPPROTO_TCP)) {
            client = argv[argc - 2];
            client_flag = 1;
        }
    }

    if (protocol == IPPROTO_TCP && !type_flag) {
        if (argc > 3) {
            printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
            exit(1);
        }
    }
    if (protocol == IPPROTO_TCP && type_flag) {
        if (client_flag) {
            if (argc > 4) {
                printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
                exit(1);
            }
        } else {
            if (argc > 3) {
                printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
                exit(1);
            }
        }
    }
    if (protocol == IPPROTO_UDP && !type_flag) {
        if (argc > 4) {
            printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
            exit(1);
        }
    }
    if (protocol == IPPROTO_UDP && type_flag) {
        if (client_flag) {
            if (argc > 5) {
                printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
                exit(1);
            }
        } else {
            if (argc > 4) {
                printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
                exit(1);
            }
        }
    }

    portno = atoi(argv[argc - 1]);
    if (portno == 0) {
        printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
        exit(1);
    }
    if (portno < 1025 || portno > 65535) {
        printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
        exit(1);
    }

    if (type_flag) { // act as server
        if (client_flag) {
            server = gethostbyname(argv[argc - 2]);
            if (server == NULL) {
                printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
                exit(1);
            }
        }
        if (protocol == IPPROTO_TCP) {
            sockfd = socket(AF_INET, sock_type, protocol);
            if (sockfd < 0)
                error();
            bzero((char *) &serv_addr, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(portno);
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            if (bind(sockfd, (struct sockaddr *) &serv_addr,
                     sizeof(serv_addr)) < 0)
                error();
            clilen = sizeof(cli_addr);
            listen(sockfd, 5);
            for (;;) {
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, (socklen_t *) &clilen);
                struct sockaddr_in *pV4Addr = &cli_addr;
                struct in_addr ipAddr = pV4Addr->sin_addr;
                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
                if ((client_flag && strcmp(str, client) == 0) || !client_flag) {
                    if (newsockfd < 0)
                        error();
                    kill_pid = fork();
                    if (kill_pid == 0) {
                        handle_server_kill_message(newsockfd, protocol);
                    }
                    pid = fork();
                    if (pid < 0)
                        error();
                    if (pid == 0) {
                        close(sockfd);
                        handle_session_tcp(newsockfd);
                        exit(0);
                    } else close(newsockfd);
                } else {
                    close(newsockfd);
                }
            }
        } else {
            char buffer[256];

            if ((sockfd = socket(AF_INET, sock_type, 0)) < 0) {
                error();
            }
            memset(&serv_addr, 0, sizeof(serv_addr));
            memset(&cli_addr, 0, sizeof(cli_addr));

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            serv_addr.sin_port = htons(portno);

            if (bind(sockfd, (const struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
                error();
            }
            kill_pid = fork();
            if (kill_pid == 0) {
                handle_server_kill_message(sockfd, protocol);
            }
            int len, n;
            for (;;) {
                n = (int) recvfrom(sockfd, (char *) buffer, 256, MSG_WAITALL, (
                        struct sockaddr *) &cli_addr, (socklen_t *) &len);
                buffer[n] = '\0';
                printf("Here is the message: %s\n", buffer);
            }
        }
    } else { //act as client
        char buffer[256];

        if (server == NULL) {
            printf("invalid or missing options\nusage: snc [-l] [-u] [hostname] port\n");
            exit(1);
        }
        if (protocol == IPPROTO_TCP) {
            sockfd = socket(AF_INET, sock_type, protocol);
            if (sockfd < 0)
                error();

            bzero((char *) &serv_addr, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            bcopy(server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
            serv_addr.sin_port = htons(portno);
            if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                error();

            read_pid = fork();
            if (read_pid == 0) {
                handle_client_read_tcp(sockfd);
            }
            for (;;) {
                bzero(buffer, 256);
                char *line = fgets(buffer, 255, stdin);
                if (line == NULL) {
                    write(sockfd, "0x03", 18);
                    close(sockfd);
                    error();
                }
                n = (int) write(sockfd, buffer, strlen(buffer));
                if (n < 0)
                    error();
                bzero(buffer, 256);
            }
        } else {
            if ((sockfd = socket(AF_INET, sock_type, 0)) < 0) {
                error();
            }

            memset(&serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(portno);
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            for (;;) {
                memset(serv_addr.sin_zero, '\0', sizeof(serv_addr.sin_zero));
                addr_size = sizeof(serv_addr);
                bzero(buffer, 256);
                char *line = fgets(buffer, 255, stdin);
                if (line == NULL) {
                    for (;;) {}
                }

                int nbytes = (int) (strlen(buffer) + 1);
                if (sendto(sockfd, buffer, (size_t) nbytes, 0, (struct sockaddr *) &serv_addr, (socklen_t) addr_size) < 0) {
                    error();
                }
            }
        }
    }
}