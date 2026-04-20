#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>      // Thư viện bắt buộc cho hàm poll()

#define MAX_FDS 1024   // Thay thế cho FD_SETSIZE của select()

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
    
    // --- 1. KHỞI TẠO CẤU TRÚC CHO POLL() ---
    struct pollfd fds[MAX_FDS];
    int nfds = 0; // Biến theo dõi số lượng FD đang thực sự hoạt động

    // Khởi tạo mảng fds với fd = -1 (poll sẽ bỏ qua các phần tử có fd âm)
    for (int i = 0; i < MAX_FDS; i++) {
        fds[i].fd = -1;
    }

    // Thêm listener socket vào đầu mảng
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    nfds = 1;

    char buf[256];
    
    // Vẫn giữ nguyên mảng ids dùng File Descriptor làm index
    char *ids[MAX_FDS] = {NULL}; 

    while (1) {
        // --- 2. GỌI HÀM POLL ---
        // timeout = -1 nghĩa là server sẽ đợi vô hạn cho đến khi có sự kiện
        int ret = poll(fds, nfds, -1);
        if (ret < 0) {
            perror("poll() failed");
            break;
        }

        // Duyệt qua các socket đang được theo dõi (chỉ duyệt đúng nfds phần tử)
        for (int p_idx = 0; p_idx < nfds; p_idx++) {
            
            // Kiểm tra xem socket này có sự kiện đọc (POLLIN) hay không
            if (fds[p_idx].revents & POLLIN) { 
                
                if (fds[p_idx].fd == listener) {
                    // --- CÓ KẾT NỐI MỚI ---
                    int client = accept(listener, NULL, NULL);
                    if (client >= 0 && client < MAX_FDS) {
                        printf("New client connected %d\n", client);
                        
                        // Thêm client mới vào cuối danh sách theo dõi của poll
                        fds[nfds].fd = client;
                        fds[nfds].events = POLLIN;
                        nfds++;

                        char *msg = "Xin chao. Hay dang nhap!\n";
                        send(client, msg, strlen(msg), 0);
                    } else if (client >= MAX_FDS) {
                        printf("Server is full, rejecting connection.\n");
                        close(client);
                    }
                } else {
                    // --- CÓ DỮ LIỆU TỪ CLIENT CŨ ---
                    
                    // Gán i = FD của client để phần logic chat bên dưới không bị thay đổi
                    int i = fds[p_idx].fd; 
                    
                    ret = recv(i, buf, sizeof(buf)-1, 0);
                    if (ret <= 0) {
                        // Client ngắt kết nối
                        printf("Client %d disconnected\n", i);
                        close(i);    
                        
                        // Dọn dẹp session đăng nhập
                        if (ids[i] != NULL) {
                            free(ids[i]);
                            ids[i] = NULL;
                        }

                        // Xóa client khỏi mảng fds bằng cách lấy phần tử cuối cùng đè lên vị trí hiện tại
                        // Đây là thao tác O(1) giúp mảng luôn liên tục
                        fds[p_idx] = fds[nfds - 1];
                        fds[nfds - 1].fd = -1;
                        nfds--;
                        
                        // Lùi index lại 1 bước để vòng lặp kiểm tra lại phần tử vừa được đưa lên đè
                        p_idx--; 

                    } else {
                        // --- LOGIC CHAT (ĐƯỢC GIỮ NGUYÊN 100%) ---
                        buf[ret] = 0;
                        printf("Received from %d: %s\n", i, buf);

                        // Xu ly dang nhap
                        if (ids[i] == NULL) {
                            // Chua dang nhap
                            // Kiem tra cu phap "client_id: client_name"
                            char cmd[32], id[32], tmp[32];
                            int n = sscanf(buf, "%s%s%s", cmd, id, tmp);
                            if (n != 2) {
                                char *msg = "Error. Thua hoac thieu tham so!\n";
                                send(i, msg, strlen(msg), 0);
                            } else {
                                if (strcmp(cmd, "client_id:") != 0) {
                                    char *msg = "Error. Sai cu phap!\n";
                                    send(i, msg, strlen(msg), 0);
                                } else {
                                    char *msg = "OK. Hay nhap tin nhan!\n";
                                    send(i, msg, strlen(msg), 0);
                                    
                                    ids[i] = malloc(strlen(id) + 1);
                                    strcpy(ids[i], id);
                                }
                            }
                        } else {
                            // Da dang nhap
                            char target[32];
                            int n = sscanf(buf, "%s", target);
                            if (n == 0) 
                                continue;
                            
                            if (strcmp(target, "all") == 0) {
                                // Chuyen tiep tin nhan cho tat ca client
                                for (int j = 0; j < MAX_FDS; j++)
                                    if (ids[j] != NULL && i != j) {
                                        send(j, ids[i], strlen(ids[i]), 0);
                                        send(j, ": ", 2, 0);
                                        char *pos = buf + strlen(target) + 1;
                                        send(j, pos, strlen(pos), 0);
                                    }
                            } else {
                                int j = 0;
                                for (; j < MAX_FDS; j++)
                                    if (ids[j] != NULL && strcmp(target, ids[j]) == 0)
                                        break;
                                if (j < MAX_FDS) {
                                    // Tim thay
                                    send(j, ids[i], strlen(ids[i]), 0);
                                    send(j, ": ", 2, 0);
                                    char *pos = buf + strlen(target) + 1;
                                    send(j, pos, strlen(pos), 0);
                                } else {
                                    // Khong tim thay
                                    continue;
                                }
                            }                                  
                        }
                    }
                }            
            }
        }
    }

    close(listener);
    // Dọn dẹp bộ nhớ trước khi thoát
    for(int i = 0; i < MAX_FDS; i++) {
        if(ids[i] != NULL) free(ids[i]);
    }
    return 0;
}