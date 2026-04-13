#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/select.h>

typedef struct {
    char id[256];
    char content[256];
    char timestamp[256];
}msg;

int check_format(char* buf, msg* client_msg){
    char* s = strtok(buf, " ");

    if(s == NULL){
        return 0; 
    }

    if(s[strlen(s)-1] != ':'){
        return 0; 
    }

    s[strlen(s)-1] = 0;
    if(strlen(s) <= 0){
        return 0; 
    } 

    memcpy(client_msg->id, s, strlen(s)+1);
    
    s = strtok(NULL, " ");
    char* s1 = strtok(NULL, " ");
    if(s1 != NULL){
        return 0; 
    }

    memcpy(client_msg->content, s, strlen(s)+1);

    return 1; 
}

int main(){
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

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
        return 1; 
    }

    if(listen(listener, 5)){
        perror("listen() failed"); 
        close(listener); 
        return 1; 
    }

    printf("Server is listening on port 8080...\n");

    char* hello_msg = "Welcome bro, bro has id:";
    char* request_msg = "Hay thong tin theo cu phap 'client_id: client_name':\n";
    char* warning = "Cu phap KHONG hop le\n"; 
    char* valid_syntax = "Cu phap hop le\n";

    fd_set fdread, fdtest;
    FD_ZERO(&fdread); 
    FD_ZERO(&fdtest); 
    FD_SET(listener, &fdread); 

    char buf[256];
    while(1){
        fdtest = fdread; 

        int ret = select(FD_SETSIZE, &fdtest, NULL, NULL, NULL);
        if(ret < 0){
            perror("select() failed"); 
            break;
        }

        if(ret == 0){
            printf("Timed out\n"); 
            continue; 
        }

        for(int i = 0; i < FD_SETSIZE; ++i){
            if(FD_ISSET(i, &fdtest)){
                if(i == listener){
                    int client = accept(listener, NULL, NULL); 
                    if(client < FD_SETSIZE){
                        printf("New client connected %d\n", client);

                        char id_norti[256]; 
                        snprintf(id_norti, sizeof(id_norti), "%s %d\n", hello_msg, client); 
                        send(client, id_norti, strlen(id_norti), 0);

                        send(client, request_msg, strlen(request_msg), 0);
                        FD_SET(client, &fdread);
                    }else{
                        printf("Oh shit, out of slots\n"); 
                        close(client); // luu y, neu khong close client van se ton tai; 
                    }
                }else{
                    ret = recv(i, buf, sizeof(buf), 0);
                    if(ret <= 0){
                        printf("Client %d disconnected\n", i); 
                        FD_CLR(i, &fdread);
                    }else{
                        buf[ret] = 0; 
                        printf("Received from %d: %s\n", i, buf);
                        msg client_msg; 
                        
                        if(check_format(buf, &client_msg)){
                            char i_msg[538];
                            snprintf(i_msg, sizeof(i_msg), "Tin nhan tu client %d %s: %s\n", i, client_msg.id, client_msg.content);

                            for(int j = 4; j < FD_SETSIZE; ++j){
                                if(j != i && FD_ISSET(j, &fdread)){
                                    send(j, i_msg, strlen(i_msg), 0);
                                }
                            }
                            // printf("%s: %s\n", client_msg.id, client_msg.content);
                            send(i,valid_syntax, strlen(valid_syntax), 0);
                        }else{
                            send(i, warning, strlen(warning), 0);
                            send(i, request_msg, strlen(request_msg), 0);
                        }  
                    }
                }
            } 
        }
    }
}