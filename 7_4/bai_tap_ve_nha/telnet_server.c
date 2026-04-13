#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#define PORT 8080
#define BACKLOG 5
#define BUF_SIZE 256

int client_auth[FD_SETSIZE]; 

void remove_newline(char *s) {
    while (*s) {
        if (*s == '\r' || *s == '\n'){ 
            *s = '\0';
            break;
        }
        s++;
    }
}

int main() {
    for (int i = 0; i < FD_SETSIZE; i++) client_auth[i] = 0;

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        return 1;
    }

    listen(listener, BACKLOG);

    printf("Telnet Server dang lang nghe tai port %d...\n", PORT);

    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_SET(listener, &master_fds);
    int max_fd = listener;

    char buf[BUF_SIZE];

    while (1) {
        read_fds = master_fds;
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) break;

        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) {
                    int client = accept(listener, NULL, NULL);
                    FD_SET(client, &master_fds);
                    if (client > max_fd) max_fd = client;
                    
                    char *welcome = "Chao mung! Hay nhap tai khoan (user pass):\n";
                    send(client, welcome, strlen(welcome), 0);
                } else {
                    int ret = recv(i, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {
                        close(i);
                        FD_CLR(i, &master_fds);
                        client_auth[i] = 0;
                    } else {
                        buf[ret] = '\0';
                        remove_newline(buf);

                        if (client_auth[i] == 0) {
                            char user[64], pass[64];
                            if (sscanf(buf, "%s %s", user, pass) == 2) {
                                FILE *f = fopen("database.txt", "r");
                                if (f == NULL) {
                                    char *msg = "Loi server: khong thay DB.\n";
                                    send(i, msg, strlen(msg), 0); 
                                    continue;
                                }
                                char f_user[64], f_pass[64];
                                int found = 0;
                                while (fscanf(f, "%s %s", f_user, f_pass) == 2) {
                                    if (strcmp(user, f_user) == 0 && strcmp(pass, f_pass) == 0) {
                                        found = 1; break;
                                    }
                                }
                                fclose(f);

                                if (found) {
                                    client_auth[i] = 1;
                                    char *msg = "Dang nhap thanh cong! Hay nhap lenh:\n";
                                    send(i, msg, strlen(msg), 0);  
                                } else {
                                    char *msg = "Sai tai khoan! Thu lai:\n";
                                    send(i, msg, strlen(msg), 0); 
                                }
                            } else {
                                char *msg = "Cu phap sai. Nhap: user pass\n";
                                send(i, msg, strlen(msg), 0); 
                            }
                        } else {
                            char cmd[BUF_SIZE + 20];
                            snprintf(cmd, sizeof(cmd), "%s > out.txt 2>&1", buf);
                            system(cmd);

                            FILE *f = fopen("out.txt", "r");
                            if (f) {
                                char line[BUF_SIZE];
                                while (fgets(line, sizeof(line), f)) {
                                    send(i, line, strlen(line), 0);
                                }
                                fclose(f);
                            }
                            char *msg = "\n--- Xong ---\n";
                            send(i, msg, strlen(msg), 0);
                        }
                    }
                }
            }
        }
    }
    return 0;
}