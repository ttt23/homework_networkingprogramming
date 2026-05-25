#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>

#define PORT 8888
#define STORAGE_DIR "./files" 
#define BUFFER_SIZE 1024

void trim_newline(char *str) {
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

void handle_client(int client_sock) {
    DIR *dir;
    struct dirent *entry;
    char buffer[BUFFER_SIZE];
    char file_list[8192] = ""; 
    char file_names[256][256];
    int file_count = 0;

    dir = opendir(STORAGE_DIR);
    if (dir == NULL) {
        perror("Không thể mở thư mục lưu trữ");
        close(client_sock);
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            strcpy(file_names[file_count], entry->d_name);
            file_count++;
        }
    }
    closedir(dir);

    if (file_count == 0) {
        char *err_msg = "ERROR No files to download\r\n";
        send(client_sock, err_msg, strlen(err_msg), 0);
        close(client_sock);
        exit(0);
    }

    sprintf(buffer, "OK %d\r\n", file_count);
    strcat(file_list, buffer);
    for (int i = 0; i < file_count; i++) {
        strcat(file_list, file_names[i]);
        strcat(file_list, "\r\n");
    }
    strcat(file_list, "\r\n"); 
    
    send(client_sock, file_list, strlen(file_list), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            break;
        }

        trim_newline(buffer);

        char file_path[sizeof(STORAGE_DIR) + BUFFER_SIZE + 2];
        snprintf(file_path, sizeof(file_path), "%s/%s", STORAGE_DIR, buffer);

        struct stat st;
        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            long file_size = st.st_size;

            char resp[128];
            sprintf(resp, "OK %ld\r\n", file_size);
            send(client_sock, resp, strlen(resp), 0);

            FILE *f = fopen(file_path, "rb");
            if (f != NULL) {
                int bytes_read;
                while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
                    send(client_sock, buffer, bytes_read, 0);
                }
                fclose(f);
            }
            
            printf("[Child %d] Đã gửi file thành công và đóng kết nối.\n", getpid());
            break; 
        } else {
            char *err_retry = "ERROR File not found. Please enter file name again\r\n";
            send(client_sock, err_retry, strlen(err_retry), 0);
        }
    }

    close(client_sock);
    exit(0);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    mkdir(STORAGE_DIR, 0777);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Không thể tạo socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind thất bại");
        exit(1);
    }

    if (listen(server_sock, 10) < 0) {
        perror("Listen thất bại");
        exit(1);
    }

    printf("File Server đang chạy trên cổng %d...\n", PORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept thất bại");
            continue;
        }

        printf("Client mới kết nối! Tạo tiến trình con xử lý...\n");

        pid_t pid = fork();
        if (pid == 0) {
            close(server_sock); 
            handle_client(client_sock);
        } else if (pid > 0) {
            close(client_sock); 
            
            while (waitpid(-1, NULL, WNOHANG) > 0);
        } else {
            perror("Fork thất bại");
            close(client_sock);
        }
    }

    close(server_sock);
    return 0;
}