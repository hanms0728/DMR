#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#define LISTEN_PORT 8888 // Secondary가 수신할 포트 번호

int main() {
    int sock;
    struct sockaddr_in server_addr, client_addr;
    char buffer[64];
    socklen_t addr_len = sizeof(client_addr);
    struct timeval tv;

    // 소켓 생성
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(LISTEN_PORT);

    // 소켓에 주소 바인딩
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return 1;
    }

    while (1) {
        // 데이터 수신
        int recv_len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client_addr, &addr_len);
        if (recv_len < 0) {
            perror("Receive failed");
            close(sock);
            return 1;
        }
        buffer[recv_len] = '\0';

        // 현재 시간 가져오기
        gettimeofday(&tv, NULL);
        long long received_time = atoll(buffer);
        long long current_time = (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;

        // 시간 차이 계산
        long long time_difference = current_time - received_time;
        printf("Time difference: %lld ms\n", time_difference);
    }

    close(sock);
    return 0;
}
