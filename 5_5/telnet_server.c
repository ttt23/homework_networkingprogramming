#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8777
#define DB_FILE "database.txt"

int check_login(char *input_user, char *input_pass) {
    FILE *f = fopen(DB_FILE, "r");
    if (f == NULL) {
        perror("Khong mo duoc file database");
        return 0;
    }

    char f_user[50], f_pass[50];
    while (fscanf(f, "%s %s", f_user, f_pass) != EOF) {
        if (strcmp(input_user, f_user) == 0 && strcmp(input_pass, f_pass) == 0) {
            fclose(f);
            return 1; 
        }
    }
    fclose(f);
    return 0; 
}

void handle_client(int client_sock) {
    char buf[1024], user[50], pass[50], tmp[1100];
    int authenticated = 0;

    while (!authenticated) {
        send(client_sock, "Nhap user pass (vi du: admin admin): ", 37, 0);
        
        int len = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (len <= 0) { close(client_sock); exit(0); }
        
        buf[len] = '\0';
        
        if (sscanf(buf, "%s %s", user, pass) == 2) {
            if (check_login(user, pass)) {
                send(client_sock, "Dang nhap thanh cong!\n", 22, 0);
                authenticated = 1;
            } else {
                send(client_sock, "Sai tai khoan hoac mat khau. Thu lai!\n", 38, 0);
            }
        } else {
            send(client_sock, "Sai cu phap! Vui long nhap theo dang: [user] [pass]\n", 52, 0);
        }
    }

    while (1) {
        send(client_sock, "$ ", 2, 0);
        int len = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (len <= 0) break;

        buf[len] = '\0';
        strtok(buf, "\r\n");

        if (strlen(buf) == 0) continue;
        if (strcmp(buf, "exit") == 0) break;

        sprintf(tmp, "%s > out.txt 2>&1", buf);
        system(tmp);

        FILE *fout = fopen("out.txt", "r");
        if (fout) {
            while (fgets(tmp, sizeof(tmp), fout) != NULL) {
                send(client_sock, tmp, strlen(tmp), 0);
            }
            fclose(fout);
        }
    }

    printf("Client ngat ket noi.\n");
    close(client_sock);
    exit(0);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("Khong tao duoc socket");
        return 1;
    }

    int optval = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("Khong cau hinh SO_REUSEADDR");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind that bai");
        close(listener);
        return 1;
    }

    if (listen(listener, 10) < 0) {
        perror("Listen that bai");
        close(listener);
        return 1;
    }
    
    signal(SIGCHLD, SIG_IGN);
    printf("Server dang doi tai port %d...\n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            perror("Accept that bai");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork that bai");
            close(client);
            continue;
        }

        if (pid == 0) {
            close(listener);
            handle_client(client);
        } else {
            close(client);
        }
    }
    return 0;
}