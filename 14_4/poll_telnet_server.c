/*******************************************************************************
 * @file    telnet_server_poll.c
 * @brief   Client dang nhap va thuc hien lenh (su dung poll)
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>       // Thay <sys/select.h> bang <poll.h>

#define MAX_FDS 1024

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
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
    
    // --- 1. KHỞI TẠO CẤU TRÚC POLL ---
    struct pollfd fds[MAX_FDS];
    int nfds = 0;

    for (int i = 0; i < MAX_FDS; i++) {
        fds[i].fd = -1;
    }

    // Đưa listener vào đầu mảng
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    nfds = 1;

    char buf[256];
    
    // Mảng lưu trạng thái login, dùng chính File Descriptor (i) làm index
    int login[MAX_FDS] = {0};

    while (1) {
        // --- 2. GỌI HÀM POLL VỚI TIMEOUT ---
        // 60 giây = 60000 mili-giây
        int ret = poll(fds, nfds, 60000); 
        
        if (ret < 0) {
            perror("poll() failed");
            break;
        }
        if (ret == 0) {
            printf("Timed out.\n");
            continue;
        }

        // Duyệt qua các socket đang được theo dõi
        for (int p_idx = 0; p_idx < nfds; p_idx++) {
            if (fds[p_idx].revents & POLLIN) {
                
                if (fds[p_idx].fd == listener) {
                    // --- CÓ KẾT NỐI MỚI ---
                    int client = accept(listener, NULL, NULL);
                    if (client >= 0 && client < MAX_FDS) {
                        printf("New client connected %d\n", client);
                        
                        // Thêm client vào mảng poll
                        fds[nfds].fd = client;
                        fds[nfds].events = POLLIN;
                        nfds++;
                        
                        // Reset trạng thái đăng nhập cho client mới
                        login[client] = 0;
                    } else if (client >= MAX_FDS) {
                        close(client);
                    }
                } else {
                    // --- CÓ DỮ LIỆU TỪ CLIENT ---
                    int i = fds[p_idx].fd; // i chính là client socket (FD)
                    
                    ret = recv(i, buf, sizeof(buf) - 1, 0); // Thêm -1 để chừa chỗ cho \0
                    if (ret <= 0) {
                        printf("Client %d disconnected\n", i);  
                        close(i);
                        login[i] = 0;
                        
                        // Dồn mảng pollfd bằng cách đưa phần tử cuối lên thay thế
                        fds[p_idx] = fds[nfds - 1];
                        fds[nfds - 1].fd = -1;
                        nfds--;
                        p_idx--; // Lùi index để vòng lặp kiểm tra lại phần tử vừa được đưa lên
                        
                    } else {
                        // --- LOGIC XỬ LÝ (GIỮ NGUYÊN 100%) ---
                        buf[ret] = 0;
                        if (buf[strlen(buf) - 1] == '\n')
                            buf[strlen(buf) - 1] = 0;
                        printf("Received from %d: %s\n", i, buf);

                        if (login[i] == 0) {
                            // Kiem tra ky tu enter
                            char user[32], pass[32], tmp[64];
                            int n = sscanf(buf, "%s%s%s", user, pass, tmp);
                            if (n != 2) {
                                char *msg = "Sai cu phap. Hay dang nhap lai.\n";
                                send(i, msg, strlen(msg), 0);
                            } else {
                                sprintf(tmp, "%s %s", user, pass);
                                int found = 0;
                                char line[64];
                                FILE *f = fopen("database.txt", "r");
                                if (f != NULL) { // Tránh lỗi crash nếu file users.txt không tồn tại
                                    while (fgets(line, sizeof(line), f) != NULL) {
                                        if (line[strlen(line) - 1] == '\n')
                                            line[strlen(line) - 1] = 0;
                                        if (strcmp(line, tmp) == 0) {
                                            found = 1;
                                            break;
                                        }
                                    }
                                    fclose(f);
                                }

                                if (found == 1) {
                                    char *msg = "OK. Hay nhap lenh.\n";
                                    send(i, msg, strlen(msg), 0);
                                    login[i] = 1;
                                } else {
                                    char *msg = "Sai username hoac password. Hay dang nhap lai.\n";
                                    send(i, msg, strlen(msg), 0);
                                }
                            }
                        } else {
                            // Thuc hien lenh tu client
                            char cmd[512];
                            sprintf(cmd, "%s > out.txt", buf);
                            system(cmd);
                            FILE *f = fopen("out.txt", "rb");
                            if (f != NULL) {
                                while (1) {
                                    int len = fread(buf, 1, sizeof(buf), f);
                                    if (len <= 0)
                                        break;
                                    send(i, buf, len, 0);
                                }
                                fclose(f);
                            }
                        }
                    }
                }            
            }
        }
    }

    close(listener);
    return 0;
}