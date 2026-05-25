#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#define PORT 8888
#define BUFFER_SIZE 1024

int process_time_command(const char *cmd, char *output, size_t max_len) {
    if (strncmp(cmd, "GET_TIME ", 9) != 0) {
        return 0;
    }

    const char *format = cmd + 9;

    time_t raw_time = time(NULL);
    struct tm *time_info = localtime(&raw_time);

    if (strcmp(format, "dd/mm/yyyy") == 0) {
        strftime(output, max_len, "%d/%m/%Y\r\n", time_info);
        return 1;
    } else if (strcmp(format, "dd/mm/yy") == 0) {
        strftime(output, max_len, "%d/%m/%y\r\n", time_info);
        return 1;
    } else if (strcmp(format, "mm/dd/yyyy") == 0) {
        strftime(output, max_len, "%m/%d/%Y\r\n", time_info);
        return 1;
    } else if (strcmp(format, "mm/dd/yy") == 0) {
        strftime(output, max_len, "%m/%d/%y\r\n", time_info);
        return 1;
    }

    return 0;
}

void *client_handler(void *socket_desc) {
    int my_sock = *(int*)socket_desc;
    free(socket_desc);

    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    int read_size;

    printf("[Server] Đã kết nối với client tại socket %d\n", my_sock);

    while ((read_size = recv(my_sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        while (read_size > 0 && (buffer[read_size - 1] == '\n' || buffer[read_size - 1] == '\r')) {
            buffer[read_size - 1] = '\0';
            read_size--;
        }

        if (strlen(buffer) == 0) continue;

        printf("[Server] Nhận lệnh từ socket %d: '%s'\n", my_sock, buffer);

        memset(response, 0, BUFFER_SIZE);
        if (process_time_command(buffer, response, BUFFER_SIZE)) {
            send(my_sock, response, strlen(response), 0);
        } else {
            char *err_msg = "ERROR Sai cu phap lenh hoac dinh dang! (VD hop le: GET_TIME dd/mm/yyyy)\r\n";
            send(my_sock, err_msg, strlen(err_msg), 0);
        }
    }

    printf("[Server] Client tại socket %d đã ngắt kết nối.\n", my_sock);
    close(my_sock);
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Lỗi tạo socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Lỗi Bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Lỗi Listen");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Time Server (Multi-thread) đang chạy tại cổng %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_size);
        if (client_fd < 0) continue;

        int *new_sock = malloc(sizeof(int));
        *new_sock = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, (void*)new_sock) == 0) {
            pthread_detach(tid); 
        } else {
            free(new_sock);
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}