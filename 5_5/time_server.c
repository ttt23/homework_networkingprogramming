#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>

#define PORT 9999

void process_time_request(int client_sock, char *format_str) {
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (strcmp(format_str, "dd/mm/yyyy") == 0) {
        strftime(buffer, sizeof(buffer), "%d/%m/%Y\n", timeinfo);
    } else if (strcmp(format_str, "dd/mm/yy") == 0) {
        strftime(buffer, sizeof(buffer), "%d/%m/%y\n", timeinfo);
    } else if (strcmp(format_str, "mm/dd/yyyy") == 0) {
        strftime(buffer, sizeof(buffer), "%m/%d/%Y\n", timeinfo);
    } else if (strcmp(format_str, "mm/dd/yy") == 0) {
        strftime(buffer, sizeof(buffer), "%m/%d/%y\n", timeinfo);
    } else {
        strcpy(buffer, "Loi: Dinh dang khong hop le!\n");
    }

    send(client_sock, buffer, strlen(buffer), 0);
}

void handle_client(int client_sock) {
    char buf[1024];
    while (1) {
        int ret = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (ret <= 0) break;

        buf[ret] = '\0';
        if (buf[ret-1] == '\n') buf[ret-1] = '\0';
        if (buf[ret-2] == '\r') buf[ret-2] = '\0';

        char cmd[20], format[50];
        int n = sscanf(buf, "%s %s", cmd, format);

        if (n == 2 && strcmp(cmd, "GET_TIME") == 0) {
            process_time_request(client_sock, format);
        } else {
            char *error_msg = "Loi: Sai cu phap. Dung: GET_TIME [format]\n";
            send(client_sock, error_msg, strlen(error_msg), 0);
        }
    }
    close(client_sock);
    exit(0);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    listen(listener, 10);

    signal(SIGCHLD, SIG_IGN);

    printf("Time Server dang chay tren port %d...\n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (fork() == 0) {
            close(listener);
            handle_client(client);
        } else {
            close(client);
        }
    }
    return 0;
}