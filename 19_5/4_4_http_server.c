#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define NUM_THREADS 4

int listener; 

void* thread_worker(void* arg) {
    int thread_id = *(int*)arg;
    free(arg);
    
    char buf[256];
    char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><body><h1>Xin chao!</h1></body></html>";

    while (1) {
        int client = accept(listener, NULL, NULL);
        
        if (client < 0) {
            perror("Loi accept");
            sleep(1); 
            continue;
        }

        printf("[Thread %d] Xu ly client socket: %d\n", thread_id, client);

        int ret = recv(client, buf, sizeof(buf) - 1, 0);
        if (ret > 0) {
            buf[ret] = 0;
            send(client, msg, strlen(msg), 0);
        }
        close(client); 
    }
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("Khong the tao socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(listener, 10) < 0) {
        perror("Listen failed");
        return 1;
    }

    printf("Server dang lang nghe tai port %d...\n", PORT);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        int *thread_id = malloc(sizeof(int));
        *thread_id = i;
        
        if (pthread_create(&threads[i], NULL, thread_worker, thread_id) != 0) {
            perror("Khong the tao thread");
            return 1;
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    close(listener);
    return 0;
}