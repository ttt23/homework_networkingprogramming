#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 8888
#define BUFFER_SIZE 2048
#define DB_FILE "users.txt"

int check_login(const char *username, const char *password) {
    FILE *file = fopen(DB_FILE, "r");
    if (file == NULL) {
        printf("[Lỗi] Không thể mở file cơ sở dữ liệu %s\n", DB_FILE);
        return 0;
    }

    char line[256];
    char db_user[128], db_pass[128];
    int authenticated = 0;

    while (fgets(line, sizeof(line), file) != NULL) {

        if (sscanf(line, "%s %s", db_user, db_pass) == 2) {
            if (strcmp(username, db_user) == 0 && strcmp(password, db_pass) == 0) {
                authenticated = 1;
                break;
            }
        }
    }

    fclose(file);
    return authenticated;
}

void send_file_content(int client_sock, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        char *err = "ERROR Không thể đọc kết quả lệnh\r\n";
        send(client_sock, err, strlen(err), 0);
        return;
    }

    char file_buf[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), file)) > 0) {
        send(client_sock, file_buf, bytes_read, 0);
    }

    fclose(file);
}

void *client_handler(void *socket_desc) {
    int my_sock = *(int*)socket_desc;
    free(socket_desc);

    char buffer[BUFFER_SIZE];
    int read_size;
    int logged_in = 0;

    char out_filename[64];
    snprintf(out_filename, sizeof(out_filename), "out_%d.txt", my_sock);

    char *welcome = "Chào mừng đến với HUST Telnet Server!\r\nVui lòng nhập tài khoản (định dạng: user pass):\r\n";
    send(my_sock, welcome, strlen(welcome), 0);

    while (!logged_in) {
        memset(buffer, 0, BUFFER_SIZE);
        read_size = recv(my_sock, buffer, BUFFER_SIZE - 1, 0);
        if (read_size <= 0) {
            close(my_sock);
            return NULL; 
        }

        while (read_size > 0 && (buffer[read_size - 1] == '\n' || buffer[read_size - 1] == '\r')) {
            buffer[read_size - 1] = '\0';
            read_size--;
        }

        if (strlen(buffer) == 0) continue;

        char username[128] = {0};
        char password[128] = {0};

        if (sscanf(buffer, "%s %s", username, password) == 2) {
            if (check_login(username, password)) {
                logged_in = 1;
                char *success_msg = "Đăng nhập THÀNH CÔNG! Hãy nhập lệnh hệ thống để thực thi:\r\n";
                send(my_sock, success_msg, strlen(success_msg), 0);
                printf("[Server] Socket %d đăng nhập thành công bằng tài khoản: %s\n", my_sock, username);
            }
        }

        if (!logged_in) {
            char *fail_msg = "Đăng nhập THẤT BẠI. Vui lòng thử lại (user pass):\r\n";
            send(my_sock, fail_msg, strlen(fail_msg), 0);
        }
    }

    while ((read_size = recv(my_sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        while (read_size > 0 && (buffer[read_size - 1] == '\n' || buffer[read_size - 1] == '\r')) {
            buffer[read_size - 1] = '\0';
            read_size--;
        }
        
        if (strlen(buffer) == 0) continue;
        
        if (strchr(buffer, ';') != NULL || strchr(buffer, '|') != NULL || strchr(buffer, '&') != NULL) {
            char *warning = "ERROR Ký tự đặc biệt không được hỗ trợ vì lý do bảo mật!\r\n";
            send(my_sock, warning, strlen(warning), 0);
            continue;
        }

        printf("[Server] Socket %d thực thi lệnh: %s\n", my_sock, buffer);

        char system_cmd[BUFFER_SIZE + 128];
        snprintf(system_cmd, sizeof(system_cmd), "%s > %s 2>&1", buffer, out_filename); 

        system(system_cmd);

        send_file_content(my_sock, out_filename);
        
        send(my_sock, "\r\n", 2, 0);
    }

    printf("[Server] Socket %d đã ngắt kết nối.\n", my_sock);
    close(my_sock);
    unlink(out_filename);

    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    FILE *f = fopen(DB_FILE, "a");
    if (f != NULL) fclose(f);

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

    printf("[Server] Telnet Server (Multi-thread) đang chạy tại cổng %d...\n", PORT);

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