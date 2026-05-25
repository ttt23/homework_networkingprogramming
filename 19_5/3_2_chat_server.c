#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 9999
#define BUFFER_SIZE 2048

typedef struct {
    int client1;
    int client2;
} ChatPair;

typedef struct {
    int from_sock;
    int to_sock;
} ForwardArgs;

void* forward_msg(void* args) {
    ForwardArgs* f_args = (ForwardArgs*)args;
    int from = f_args->from_sock;
    int to = f_args->to_sock;
    free(f_args);

    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(from, buffer, sizeof(buffer), 0)) > 0) {
        send(to, buffer, bytes_received, 0);
    }

    printf("[Server] Một client đã ngắt kết nối. Đóng cả cặp chat...\n");
    
    close(from);
    close(to);

    return NULL;
}

void start_chat_session(int c1, int c2) {
    printf("[Server] Ghép đôi thành công: Socket %d <==> Socket %d\n", c1, c2);

    char* welcome_msg = "SUCCESS Bạn đã được kết nối với bạn chat! Hãy bắt đầu trò chuyện.\r\n";
    send(c1, welcome_msg, strlen(welcome_msg), 0);
    send(c2, welcome_msg, strlen(welcome_msg), 0);

    pthread_t thread1, thread2;

    ForwardArgs* args1 = malloc(sizeof(ForwardArgs));
    args1->from_sock = c1;
    args1->to_sock = c2;
    pthread_create(&thread1, NULL, forward_msg, (void*)args1);
    pthread_detach(thread1); 

    ForwardArgs* args2 = malloc(sizeof(ForwardArgs));
    args2->from_sock = c2;
    args2->to_sock = c1;
    pthread_create(&thread2, NULL, forward_msg, (void*)args2);
    pthread_detach(thread2);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    int waiting_client = -1; 
    pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    printf("[Server] Chat Server đang chạy tại port %d. Chờ các client kết nối...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_size);
        if (client_fd < 0) continue;

        printf("[Server] Client mới kết nối (Socket: %d)\n", client_fd);

        pthread_mutex_lock(&queue_mutex);

        if (waiting_client == -1) {
            waiting_client = client_fd;
            char* wait_msg = "WAIT Đang tìm kiếm bạn chat, vui lòng đợi...\r\n";
            send(client_fd, wait_msg, strlen(wait_msg), 0);
            printf("[Server] Socket %d đang ở hàng đợi.\n", client_fd);
        } else {
            int client1 = waiting_client;
            int client2 = client_fd;
            
            waiting_client = -1; 

            start_chat_session(client1, client2);
        }

        pthread_mutex_unlock(&queue_mutex);
    }

    close(server_fd);
    return 0;
}