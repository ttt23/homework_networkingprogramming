#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#define PORT 8888
#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048

typedef struct {
    int socket;
    char id[50];
    int is_authenticated;
} ClientType;

ClientType clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void get_current_time_str(char *time_str, size_t max_len) {
    time_t raw_time = time(NULL);
    struct tm *time_info = localtime(&raw_time);
    strftime(time_str, max_len, "%Y/%m/%d %I:%M:%S%p", time_info);
}

void broadcast_message(const char *message, int sender_sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != sender_sock && clients[i].is_authenticated == 1) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == sock) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *client_handler(void *socket_desc) {
    int my_sock = *(int*)socket_desc;
    free(socket_desc);

    char buffer[BUFFER_SIZE];
    char client_id[50];
    int read_size;
    int authenticated = 0;

    char *welcome = "HUST CHATROOM: Vui long dang nhap theo cu phap 'client_id: client_name'\r\n";
    send(my_sock, welcome, strlen(welcome), 0);

    while (!authenticated) {
        memset(buffer, 0, BUFFER_SIZE);
        read_size = recv(my_sock, buffer, BUFFER_SIZE - 1, 0);
        if (read_size <= 0) {
            close(my_sock);
            remove_client(my_sock);
            return NULL; 
        }

        while (read_size > 0 && (buffer[read_size - 1] == '\n' || buffer[read_size - 1] == '\r')) {
            buffer[read_size - 1] = '\0';
            read_size--;
        }

        char *colon = strchr(buffer, ':');
        if (colon != NULL) {
            // Trích xuất client_id
            *colon = '\0'; 
            char *temp_id = buffer;
            
            while(*temp_id == ' ') temp_id++;
            
            if (strlen(temp_id) > 0) {
                strcpy(client_id, temp_id);
                authenticated = 1;

                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < client_count; i++) {
                    if (clients[i].socket == my_sock) {
                        strcpy(clients[i].id, client_id);
                        clients[i].is_authenticated = 1;
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);

                char success_msg[128];
                snprintf(success_msg, sizeof(success_msg), "SUCCESS Dang nhap thanh cong voi ID: %s\r\n", client_id);
                send(my_sock, success_msg, strlen(success_msg), 0);
                
                printf("[Server] Client tai socket %d da dang ky ID: %s\n", my_sock, client_id);
            }
        }

        if (!authenticated) {
            char *err_syntax = "ERROR Sai cu phap! Vui long gui lai dang 'client_id: client_name'\r\n";
            send(my_sock, err_syntax, strlen(err_syntax), 0);
        }
    }

    while ((read_size = recv(my_sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        while (read_size > 0 && (buffer[read_size - 1] == '\n' || buffer[read_size - 1] == '\r')) {
            buffer[read_size - 1] = '\0';
            read_size--;
        }
        if (strlen(buffer) == 0) continue;

        char time_str[50];
        get_current_time_str(time_str, sizeof(time_str));

        char broadcast_buf[BUFFER_SIZE + 256];
        snprintf(broadcast_buf, sizeof(broadcast_buf), "%s %s: %s\r\n", time_str, client_id, buffer);

        broadcast_message(broadcast_buf, my_sock);
    }

    printf("[Server] Client %s (Socket %d) da ngat ket noi.\n", client_id, my_sock);
    close(my_sock);
    remove_client(my_sock);

    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Loi tao socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Loi Bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Loi Listen");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Chatroom Server (Multi-thread) dang chay tai port %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_size);
        if (client_fd < 0) continue;

        pthread_mutex_lock(&clients_mutex);
        if (client_count < MAX_CLIENTS) {
            clients[client_count].socket = client_fd;
            clients[client_count].is_authenticated = 0;
            memset(clients[client_count].id, 0, 50);
            client_count++;

            int *new_sock = malloc(sizeof(int));
            *new_sock = client_fd;

            pthread_t tid;
            if (pthread_create(&tid, NULL, client_handler, (void*)new_sock) == 0) {
                pthread_detach(tid); 
            } else {
                free(new_sock);
                close(client_fd);
            }
        } else {
            char *full_msg = "ERROR Server hien tai da day phong chat!\r\n";
            send(client_fd, full_msg, strlen(full_msg), 0);
            close(client_fd);
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    close(server_fd);
    return 0;
}