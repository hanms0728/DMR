#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/timeb.h>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/stat.h>
#include <sys/reboot.h>

int main(){
    char snd_buff[10];

    struct timeb milli_now;
    struct tm *now;

    ftime(&milli_now);
    now = localtime(&milli_now.time);

    int sockfd;

    // 소켓 포트 설정
	socklen_t clnt_leng;
	int usingPort = 9999;
	bool bValid = 1;
	struct sockaddr_in servaddr, clntaddr;

    // UDP 소켓 생성
	sockfd = socket(AF_INET, SOCK_DGRAM, 0); //소켓 생성
	if (sockfd < 0)
		error_handling("\nUDP socket() error...\n");

	printf("\nUDP Socket is opened as Server...\n");

	// 소켓 유지 옵션 (재부팅 및 중복 사용)
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&bValid, sizeof(bValid)); //소켓 초기 설정

	// 소켓 타임아웃 세부 설정
	struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 10 * 1000;

	// 소켓 송수신 타임아웃 설정(Non-Blocking)
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout); //rcv 제한 시간
	//setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &read_timeout, sizeof read_timeout);

	// 소켓 기본 설정
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; // IPv4 
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(usingPort);

    while(1){
        // HB 버퍼 메모리 선언
		memset(&snd_buff, 0, sizeof(char) * 10);

		int n;
		clnt_leng = sizeof(clntaddr);

        ftime(&milli_now);
        now = localtime(&milli_now.time);
        
        sprintf(snd_buff, "%ld", (now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);


        // HB 송신
		sendto(sockfd, snd_buff, 10, 0, (struct sockaddr*)&clntaddr, clnt_leng);

        usleep(10000);
    }

}