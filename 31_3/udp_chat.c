#include<stdio.h> 
#include<string.h> 
#include<stdlib.h> 
#include<sys/types.h> 
#include<sys/socket.h> 
#include<arpa/inet.h> 
#include<unistd.h>
#include<fcntl.h> 
#include<errno.h> 

int main(int argc, char* argv[]){ 
    int port_s = strtol(argv[1], NULL, 10); 
    int port_d = strtol(argv[3], NULL, 10); 
    char* ip_d = argv[2];

    int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    struct sockaddr_in source_addr; 
    source_addr.sin_family = AF_INET; 
    source_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    source_addr.sin_port = htons(port_s);
    bind(socket_fd, (struct sockaddr*)&source_addr, sizeof(source_addr)); 

    struct sockaddr_in des_addr;
    des_addr.sin_family = AF_INET;
    des_addr.sin_addr.s_addr = inet_addr(ip_d);
    des_addr.sin_port = htons(port_d);

    // Dat socket o che do non-blocking; 
    fcntl(socket_fd, F_SETFL, O_NONBLOCK); 
    // Dat stdin o che do non-blocking;
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    char send_buf[256]; 
    char recv_buf[256]; 
    while(1){
        // Gui du lieu; 
        // printf("Enter message: ");
        if(fgets(send_buf, sizeof(send_buf), stdin) != NULL){
            send_buf[strcspn(send_buf, "\n")]='\0';
            sendto(socket_fd, send_buf, strlen(send_buf), 0, (struct sockaddr*)&des_addr, sizeof(des_addr));
        }

        // Nhan du lieu; 
        // printf("0\n"); 
        int ret = recvfrom(socket_fd, recv_buf, sizeof(recv_buf)-1, 0, NULL, NULL);
        // printf("1\n"); 
        if(ret > 0){
            // printf("1\n");
            recv_buf[ret] = '\0';
            printf("Receive %d bytes: %s\n", ret, recv_buf); 
        }
    }

    close(socket_fd);
    return 0; 
}