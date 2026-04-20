#include<stdio.h> 
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/socket.h>
#include<poll.h>

#define MAX_CLIENT 1000

int main(){
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if(listener < 0){
        perror("socket() failed");
        exit(1);
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if(bind(listener, (struct sockaddr*)&addr, sizeof(addr))){
        perror("bind() failed");
        close(listener);
        exit(1);
    }

    if(listen(listener, 5)){
        perror("listen() failed");
        close(listener);
        exit(1);
    }

    printf("----Listening on port 8080----\n");

    struct pollfd fds[MAX_CLIENT];
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    int nfds = 1; 
    char buf[256];
    while(1){
        // t_nfds = nfds;
        int ret = poll(fds, nfds, -1);

        if(ret == -1){
            perror("poll() failed");
            break;
        }

        if(ret == 0){
            printf("Timed out\n");
            continue;
        }

        for(int i = 0; i < nfds; ++i){
            if(fds[i].revents & POLLIN){
                if(fds[i].fd == listener){
                    int client = accept(listener, NULL, NULL);
                    if(nfds < MAX_CLIENT){
                        fds[nfds].fd = client;
                        fds[nfds].events = POLLIN;
                        fds[nfds].revents = 0;
                        ++nfds;
                        printf("Client %d connected\n", client);
                    }else{
                        printf("Oh shit! Out of slot\n");
                        close(client);
                    }
                }else{
                    ret = recv(fds[i].fd, buf, sizeof(buf)-1, 0);
                    if(ret <= 0){
                        close(fds[i].fd);
                        printf("Client %d disconnected\n", fds[i].fd);
                        fds[i] = fds[nfds-1];
                        --nfds;
                        --i; 
                        // printf("Client %d disconnected\n", i);
                    }else{
                        buf[ret] = 0;
                        buf[strcspn(buf, "\r\n")] = 0;
                        printf("Received %d bytes from client %d: %s\n", ret, fds[i].fd, buf);
                    }
                }
            }
        }
    }

    close(listener);
    return 0;
}