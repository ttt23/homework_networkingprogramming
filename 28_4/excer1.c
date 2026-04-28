#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<poll.h>

#define MAXCLIENT 1024

void encrypt_str(char* buf, char* res){
    int len = strlen(buf);
    for(int i = 0; i < len; ++i){
        if(buf[i] >= 'a' && buf[i] <= 'z'){
            if(buf[i] == 'z'){
                res[i] = 'a';
            }else{
                res[i] = buf[i] + 1;
            }
        }
        else if(buf[i] >= 'A' && buf[i] <= 'Z'){
            if(buf[i] == 'Z'){
                res[i] = 'A';
            }else{
                res[i] = buf[i]+1;
            }
        }
        else if(buf[i] >= '0' && buf[i] <= '9'){
            res[i] = '9' - buf[i] + '0';
        }
        else{
            res[i] = buf[i];
        }
    }
    res[len] = '\n';
    res[len+1] = 0;
}

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

    printf("Server is listening on port 8080...\n");

    struct pollfd fds[MAXCLIENT];
    int nfds = 0;
    
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    nfds += 1;

    char buf[256], res[256];
    buf[255] = 0;
    while(1){
        int ret = poll(fds, nfds, -1);
        if(ret < 0){
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
                    if(nfds < MAXCLIENT){
                        fds[nfds].fd = client;
                        fds[nfds].events = POLLIN;
                        ++nfds;
                        printf("New client %d connected\n", client);

                        char message[256];
                        sprintf(message, "%s %d %s", "Xin chao. Hien co", nfds-1, "dang ket noi\n");
                        send(client, message, strlen(message), 0);
                    }else{
                        printf("Oops! Out of slots\n");
                        close(client);
                    }
                }else{
                    ret = recv(fds[i].fd, buf, sizeof(buf)-1, 0);
                    buf[strcspn(buf, "\r\n")] = 0;

                    if(ret <= 0){
                        close(fds[i].fd);
                        printf("Client %d disconnected\n", fds[i].fd);

                        fds[i] = fds[--nfds];
                        --i;

                        // printf("Client %d disconnected\n", fds[i].fd);
                    }else{
                        if(strncmp(buf, "exit", 4) == 0){
                            close(fds[i].fd);
                            printf("Client %d disconnected\n", fds[i].fd);

                            fds[i] = fds[--nfds];
                            --i;

                            // printf("Client %d disconnected\n", fds[i].fd);
                        }else{
                            encrypt_str(buf, res);
                            send(fds[i].fd, res, strlen(res), 0);
                        }
                    }
                }
            }
        }
    }
    close(listener);
    return 0; 
}