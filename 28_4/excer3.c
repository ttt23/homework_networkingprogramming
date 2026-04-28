#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<poll.h>

#define MAXCLIENT 1024
#define MAXTOPIC 32

struct topic{
    char name[20];
    unsigned int code;
};

struct client{
    int sockfd;
    unsigned int topic;
};

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

    // Quan ly cac topic;
    char temp[20] = "null";
    struct topic topic_list[MAXTOPIC];
    for(int i = 0; i < MAXTOPIC; ++i){
        memcpy(topic_list[i].name, temp, 20);
        topic_list[i].code = 0;
    }
    
    // TO-DO: write function to add other topics and its code;
    strcpy(topic_list[0].name, "the_thao");
    topic_list[0].code = 1U << 0;
    
    strcpy(topic_list[1].name, "chinh_tri");
    topic_list[1].code = 1U << 1;

    strcpy(topic_list[2].name, "giai_tri");
    topic_list[2].code = 1U << 2;

    strcpy(topic_list[3].name, "giao_duc");
    topic_list[3].code = 1U << 3;


    // Quan ly cac client;
    struct client client_list[MAXCLIENT];
    for(int i = 0; i < MAXCLIENT; ++i){
        client_list[i].sockfd = -1;
        client_list[i].topic = 0U;
    }

    struct pollfd fds[MAXCLIENT];
    for(int i = 0; i < MAXCLIENT; ++i){
        fds[i].fd = -1;
    }
    int nfds = 0;
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    client_list[0].sockfd = listener;
    nfds += 1;

    char buf[256];
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
                        client_list[nfds].sockfd = client;
                        ++nfds;
                        printf("New client %d connected\n", client);
                    }else{
                        printf("Oops! Out of slots\n");
                        close(client);
                    }
                }else{
                    ret = recv(fds[i].fd, buf, sizeof(buf)-1, 0);
                    buf[ret] = 0;

                    if(ret <= 0){
                        close(fds[i].fd);
                        printf("Client %d disconnected\n", fds[i].fd);

                        fds[i] = fds[--nfds];
                        client_list[i] = client_list[nfds];
                        client_list[nfds].topic = 0U;
                        --i;
                    }
                    else{
                        printf("Received %d bytes from client %d: %s", ret, fds[i].fd, buf);
                        char cmd[20], topic[20];
                        memset(cmd, 0, sizeof(cmd));
                        memset(topic, 0, sizeof(topic));
                        sscanf(buf, "%s%s", cmd, topic);
                        
                        //TO-DO: write function to check if cmd and topic are valid; 

                        unsigned int topic_code=0U;
                        char topic_name[20];
                        for(int j = 0; j < MAXTOPIC; ++j){
                            if(strcmp(topic, topic_list[j].name) == 0){
                                topic_code = topic_list[j].code;
                                strcpy(topic_name, topic_list[j].name);
                                break;
                            }
                        }

                        if(strcmp(cmd, "SUB") == 0){
                            client_list[i].topic |= topic_code;
                        }
                        else if(strcmp(cmd, "UNSUB") == 0){
                            client_list[i].topic &= (topic_code^0xFFFFFFFF);
                        }
                        else if(strcmp(cmd, "PUB") == 0){

                            //TO-DO: use strtok to retrieve content in general case. Here, two spaces are default;
                            char* content = buf+strlen(cmd)+strlen(topic)+2;
                            char msg[256];
                            sprintf(msg, "[%s] %s", topic_name, content);
                            for(int j = 0; j < nfds; ++j){
                                if(client_list[j].topic & topic_code){
                                    send(client_list[j].sockfd, msg, strlen(msg), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0; 
}