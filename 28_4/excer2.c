#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<poll.h>

int main(int argc, char* argv[]){
    int s_port = atoi(argv[1]);
    char* r_ip = argv[2];
    int r_port = atoi(argv[3]);

    // printf("%d %s %d\n", s_port, r_ip, r_port);
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("socket() failed");
        exit(1);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(sockfd);
        exit(1);
    }

    struct sockaddr_in s_addr = {0};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(s_port);

    if(bind(sockfd, (struct sockaddr*)&s_addr, sizeof(s_addr))){
        perror("bind() failed");
        close(sockfd);
        exit(1);
    }

    struct sockaddr_in r_addr = {0};
    r_addr.sin_family = AF_INET;
    r_addr.sin_addr.s_addr = inet_addr(r_ip);
    r_addr.sin_port = htons(r_port);

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    char buf[256];
    buf[255] = 0;
    while(1){
        int ret = poll(fds, 2, -1);
        if(ret < 0){
            perror("poll() failed");
            break;
        }

        if(ret == 0){
            printf("Timed out\n");
            continue;
        }

        if(fds[0].revents & POLLIN){
            fgets(buf, sizeof(buf), stdin);
            sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&r_addr, sizeof(r_addr));
        }

        if(fds[1].revents & POLLIN){
            ret = recvfrom(sockfd, buf, sizeof(buf)-1, 0, NULL, NULL);
            buf[strcspn(buf, "\r\n")] = 0;
            printf("Received %d bytes: %s\n", ret, buf);
        }
    }
    
    close(sockfd);
    return 0;
}