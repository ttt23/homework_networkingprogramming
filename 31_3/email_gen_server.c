#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>

// Struct quan ly trang thai cua tung client
typedef struct {
    int fd;
    int state;       // 0: Dang cho nhap Ten, 1: Dang cho nhap MSSV, 2: Hoan thanh nhap ten va mssv
    char name[256];  // Luu tru tam thoi ten cua client
    char mssv[256]; // Luu tru tam thoi mssv cua client
} ClientInfo;

// Ham xu ly chuoi de tao email
void generate_email(char *name, char *mssv, char *email_out) {
    // Chuyen ten thanh chu thuong
    for (int i = 0; name[i]; i++) {
        name[i] = tolower(name[i]);
    }

    // Tach cac tu trong ten
    char *words[20];
    int count = 0;
    char *token = strtok(name, " \t\r\n"); // Loai bo khoang trang, xuong dong
    while (token != NULL) {
        words[count++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    if (count == 0) {
        strcpy(email_out, "Loi: Ten khong hop le.\n");
        return;
    }

    // Tao prefix ten
    char prefix[256] = "";
    strcpy(prefix, words[count - 1]); // Lay ten chinh (tu cuoi cung)
    strcat(prefix, ".");
    for (int i = 0; i < count - 1; i++) {
        int len = strlen(prefix);
        prefix[len] = words[i][0]; // Lay chu cai dau cua cac ho/dem
        prefix[len + 1] = '\0';
    }

    // Xu ly MSSV (VD: 20231234 -> 231234)
    char clean_mssv[64] = "";
    char *m_token = strtok(mssv, " \t\r\n");
    if (m_token != NULL) {
        if (strlen(m_token) == 8) {
            strcpy(clean_mssv, m_token + 2); // Bo 2 ki tu dau neu mssv co 8 ki tu
        } else {
            strcpy(clean_mssv, m_token); // Giu nguyen neu do dai khac
        }
    }

    // Ghep thanh email hoan chinh
    sprintf(email_out, "Email cua ban la: %s%s@sis.hust.edu.vn\n", prefix, clean_mssv);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    // Chuyen socket listener sang non-blocking
    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Server is listening on port 8080...\n");

    // Thay mang int bang mang Struct de quan ly trang thai
    ClientInfo clients[64];
    int nclients = 0;
    char buf[256];

    while (1) {
        // Chap nhan ket noi
        int client_fd = accept(listener, NULL, NULL);
        if (client_fd != -1) {
            printf("New client accepted: %d\n", client_fd);
            
            // Chuyen client moi sang non-blocking
            ul = 1;
            ioctl(client_fd, FIONBIO, &ul);

            // Them vao danh sach quan ly
            clients[nclients].fd = client_fd;
            clients[nclients].state = 0; // Trang thai ban dau
            memset(clients[nclients].name, 0, sizeof(clients[nclients].name));
            nclients++;

            // Gui loi chao va yeu cau nhap ten
            char *msg = "Hay nhap ten: ";
            send(client_fd, msg, strlen(msg), 0);
        }

        // Nhan va xu ly du lieu tu cac client
        for (int i = 0; i < nclients; i++) {
            int len = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);
            
            if (len == -1) {
                if (errno != EWOULDBLOCK) {
                    
                }else{
                    continue;
                }
                
            } 
            else if (len == 0) {
                continue;
            } 
            else {
                buf[len] = '\0'; // Dam bao chuoi ket thuc dung

                // XU LY STATE MACHINE
                if (clients[i].state == 0) {
                    // Dang o buoc nhap Ten
                    strcpy(clients[i].name, buf);
                    clients[i].state = 1; // Chuyen sang buoc tiep theo
                    
                    char *msg = "Hay nhap mssv: ";
                    send(clients[i].fd, msg, strlen(msg), 0);
                } 
                else if (clients[i].state == 1) {
                    // Dang o buoc nhap MSSV
                    clients[i].state = 2;
                    strcpy(clients[i].mssv, buf); 
                    char email_result[512];
                    generate_email(clients[i].name, clients[i].mssv, email_result);
                    
                    // Ghi ket qua tra ve
                    send(clients[i].fd, email_result, strlen(email_result), 0);
                    
                }else{
                    char* tbao = "Server da hoan thanh nhiem vu"; 
                    printf("Received from %d: %s%d\n", clients[i].fd, buf, len);
                    send(clients[i].fd, tbao, strlen(tbao), 0); 
                }
            }
        }
    }

    close(listener);
    return 0;
}