#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8080
#define NUM_PROCESSES 5

void handle_http_request(int client_sock) {
    char buf[1024];
    int ret = recv(client_sock, buf, sizeof(buf) - 1, 0);
    if (ret <= 0) {
        close(client_sock);
        return;
    }
    buf[ret] = 0;
    printf("--- Request received by Process %d ---\n%s\n", getpid(), buf);

    char *msg = "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 45\r\n"
                "Connection: close\r\n\r\n"
                "<html><body><h1>Xin chao cac ban!</h1></body></html>";
    
    send(client_sock, msg, strlen(msg), 0);
    
    close(client_sock);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    
    listen(listener, 10);
    printf("Preforking HTTP Server started on port %d with %d workers...\n", PORT, NUM_PROCESSES);

    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (fork() == 0) {
            while (1) {
                int client = accept(listener, NULL, NULL);
                if (client < 0) continue;

                handle_http_request(client);
            }
        }
    }

    signal(SIGCHLD, SIG_IGN); 
    while (1) {
        pause(); 
    }

    return 0;
}