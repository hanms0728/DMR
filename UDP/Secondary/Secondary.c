#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/stat.h>
#include <sys/reboot.h>

#include <inttypes.h>
#include <math.h>
#define _POSIX_C_SOURCE 200809L

#define BUFSIZE 100 // 버퍼 사이즈
#define HBSIZE 5 // Primary상태(1) + 현재 HB주기(3) + 널문자(1)
#define HBCHECK 3 // HB 수신 체크 횟수

// 미션 프로그램 절대경로
// #define MSDIR "/home/pi/automotive/basicmath/basicmath_large"
// #define MSNAME "basicmath_large"
#define MSDIR "./basicmath_large"
#define MSNAME "basicmath_large"

//Primary System IP 타겟팅
#define PRIIP "10.0.0.1" //D-UDP
//#define PRIIP "192.168.0.104" //Wlan

typedef struct Control {
	int mission_status; // 본 시스템 미션 동작 상태
	int opponent_status; // 0: 상대 시스템 죽음, 1: 상대 시스템 살아있음, 2: 상대 시스템 미션 수행 중
	int reboot_status; // 0: reboot 된 적 없음, 1: reboot 되었음
	int Error_Detection;
} CT;
CT ct;

void* Heartbeat(void* arg);
void* Profiler(void* arg);
void* Mission(void* arg);
void error_handling(char* message);
pthread_mutex_t mutx;


//trace를 위한 함수
void receive_heartbeat() { return ;}




int HB_INTERVAL = 0;
int HB_INTERVAL_ns = 0;
int Test_Count = 1;
int Error_type = 0;

struct timeb milli_now;
struct tm *now;	

int main(int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s arg1 \n", argv[0]);
		return 1;
	}
	HB_INTERVAL = atoi(argv[1]); // HB주기 값
	HB_INTERVAL_ns = HB_INTERVAL * 1000;
	
	ct.Error_Detection = 1;
	

	printf("Secondary system is starting...\n");

	sleep(1);

	pthread_t pt_hb, pt_m, pt_p;
	void* pt_result;

	// mutex 생성
	if (pthread_mutex_init(&mutx, NULL))
		error_handling("mutex init error");

	// 하트비트 스레드 생성
	printf("\nCreate HB thread...\n");
	pthread_create(&pt_hb, NULL, Heartbeat, NULL);

	// 프로파일러 스레드 생성
	//printf("\nCreate Profiler thread...\n");
	//pthread_create(&pt_p, NULL, Profiler, NULL);

	sleep(1);

	while (1)
	{
		// 미션 대체 실행
		if (ct.opponent_status == 0 && ct.mission_status == 0) // 상대 시스템 죽음 && 본 시스템에서 미션 수행 X
		{
			printf("\nCreate Mission thread...\n");
			printf("\nReplacement command execute...\n");
			printf("start %s", MSNAME);

			pthread_create(&pt_m, NULL, Mission, NULL);
			//pthread_join(pt_m, &pt_result);
			
			pthread_mutex_lock(&mutx);
	        ct.mission_status = 1;
	        ct.reboot_status = 0; // 본 시스템에서 미션을 수행했으니, 나중에 reboot 할 수도 있도록 값 수정
	        pthread_mutex_unlock(&mutx);
		      
		}

		if (ct.opponent_status == 2 && ct.reboot_status == 0) // 상대 시스템이 work 상태인 경우
		{
			printf("Kill Mission Program & Reset System.\n");

			// 실행 중인 미션 프로그램 kill
			char __buff[BUFSIZE];
			sprintf(__buff, "sudo pkill %s", MSNAME);
			system(__buff);
			pthread_mutex_lock(&mutx);
			ct.Error_Detection = 0;
			ct.mission_status = 0;
			ct.reboot_status = 1;
			pthread_mutex_unlock(&mutx);
		}

	}

	// 이 코드는 프로그램 구조 상 실행 되지 않음
	/*
	pthread_join(pt_hb, &pt_result);
	pthread_join(pt_p, &pt_result);
        */
	printf("\nPrimary system is shut down...\n");

	return 0;
}

void* Heartbeat(void* arg)
{
	int hb_check; // HB 미수신 3회 체크
	int hb_active; // 상대 시스템의 미션 수행 상태
	int hb_state; // HB 통신 연결 상태

	char snd_buff[HBSIZE]; // HB 송신 버퍼
	char rcv_buff[HBSIZE]; // HB 수신 버퍼

	// 소켓 생성 변수
	int sockfd;

	int n;

	// 소켓 포트 설정
	socklen_t clnt_leng;
	int usingPort = 9999;
	bool bValid = 1;
	struct sockaddr_in servaddr;

	// UDP 소켓 생성
	sockfd = socket(AF_INET, SOCK_DGRAM, 0); //소켓 생성
	if (sockfd < 0)
		error_handling("UDP socket() error");

	printf("\nUDP Socket is opened as Client...\n");

	// 소켓 유지 옵션 (재부팅 및 중복 사용)
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&bValid, sizeof(bValid)); //소켓 초기 설정

	// 소켓 타임아웃 세부 설정
	struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = HB_INTERVAL_ns;

	// 소켓 송수신 타임아웃 설정(Non-Blocking)
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout); //rcv 제한 시간

	// 소켓 기본 설정
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(usingPort);
	if (inet_pton(AF_INET, PRIIP, &servaddr.sin_addr) <= 0)
		error_handling("inet_pton() error");

	while (1) {
		// HB 버퍼 메모리 선언
		memset(&snd_buff, 0, sizeof(char) * HBSIZE);
		memset(&rcv_buff, 0, sizeof(char) * HBSIZE);

		int n;

		//Primary로부터 할당받는 주소(포트)
		struct sockaddr_in fromaddr;
		socklen_t fromaddrlen = sizeof(fromaddr);

		// HB 송신
		sendto(sockfd, snd_buff, HBSIZE, 0, (struct sockaddr*)&servaddr, sizeof(servaddr));

		// HB 수신 불가 시
		
		n = recvfrom(sockfd, rcv_buff, HBSIZE, 0, (struct sockaddr*)&fromaddr, &fromaddrlen);
		receive_heartbeat();
		if(n <= 0 && hb_check < HBCHECK)
		{
		        //printf("no heart beat signal\n");
			hb_check++;	// HB 미수신 체크 횟수 증가
		}


		// HB 3회 미수신
		if (hb_check >= HBCHECK)
		{
			hb_state = 0; // 0: HB 통신 연결 불가
			hb_check = 0; // HB Check 초기화
		}

		//HB 수신
		if (n > 0)
		{ 
		          
			hb_state = 1;
			hb_check = 0; // HB Check 초기화

			HB_INTERVAL = atoi(rcv_buff + 1); //Primary 보드가 보낸 HB_INTERVAL와 동일하게 동기화
			HB_INTERVAL_ns = HB_INTERVAL * 1000;



			// "alive" 수신
			if (rcv_buff[0] == 'a')
			{
				pthread_mutex_lock(&mutx);
				ct.opponent_status = 1;
				pthread_mutex_unlock(&mutx);
			}

			// "work!" 수신
			if (rcv_buff[0] == 'w')
			{
				pthread_mutex_lock(&mutx);
				ct.opponent_status = 2;
				pthread_mutex_unlock(&mutx);
			}

			// "memory" 수신
			if (rcv_buff[0] == 'h')
			{

				ftime(&milli_now);
    			now = localtime(&milli_now.time);
				
				Error_type = 2;
				
				if(ct.Error_Detection == 0 && ct.opponent_status == 2){
					FILE* fp = fopen("./detection.csv", "a+");
					fprintf(fp, "HB_Interval = %d,Test_Count = %d,heat!,%d-%d-%d %d:%d:%d.%d, %dms\n", HB_INTERVAL,Test_Count,
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
					(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
					fclose(fp);
				}

				pthread_mutex_lock(&mutx);
				ct.opponent_status = 0; // 1: 미션 대체 실행 필요
				pthread_mutex_unlock(&mutx);

				printf("\n(HB thread) Received 'heat!' signal\n");
			}

			// "memory" 수신
			if (rcv_buff[0] == 'm')
			{
				ftime(&milli_now);
    			now = localtime(&milli_now.time);

				Error_type = 3;

				if(ct.Error_Detection == 0 && ct.opponent_status == 2){
					FILE* fp = fopen("./detection.csv", "a+");
					fprintf(fp, "HB_Interval = %d,Test_Count = %d, memory,%d-%d-%d %d:%d:%d.%d, %dms\n", HB_INTERVAL,Test_Count,
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
					(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
					fclose(fp);
				}

				pthread_mutex_lock(&mutx);
				ct.opponent_status = 0; // 1: 미션 대체 실행 필요
				pthread_mutex_unlock(&mutx);

				printf("\n(HB thread) Received 'memory' signal\n");
			}

			// "voltage" 수신
			if (rcv_buff[0] == 'v')
			{
				ftime(&milli_now);
    			now = localtime(&milli_now.time);

				Error_type = 4;

				if(ct.Error_Detection == 0 && ct.opponent_status == 2){
					FILE* fp = fopen("./detection.csv", "a+");
					fprintf(fp, "HB_Interval = %d,Test_Count = %d, voltage,%d-%d-%d %d:%d:%d.%d, %dms\n", HB_INTERVAL,Test_Count,
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
					(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
					fclose(fp);
				}

				pthread_mutex_lock(&mutx);
				ct.opponent_status = 0; // 1: 미션 대체 실행 필요
				pthread_mutex_unlock(&mutx);

				printf("\n(HB thread) Received 'voltage' signal\n");
			}


		}

		// HB 통신 불가능
		if (0 == hb_state) // HB 신호 미수신
		{
			if (ct.Error_Detection == 0 && ct.opponent_status == 2) {

				ftime(&milli_now);
    			now = localtime(&milli_now.time);

				ct.Error_Detection = 1;

				FILE* fp = fopen("./detection.csv", "a+");
				fprintf(fp, "HB_Interval = %d,Test_Count = %d,no_hb_signal,%d-%d-%d, %d:%d:%d.%d, %dms\n", HB_INTERVAL,Test_Count,
				now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
				(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
				fclose(fp);
			}

			pthread_mutex_lock(&mutx);
			ct.opponent_status = 0; // 1: 미션 대체 실행 필요
			pthread_mutex_unlock(&mutx);

			//printf("\n(HB thread) No HB Signals\n");

		}

		usleep(HB_INTERVAL_ns); // HB 간격만큼 딜레이
	}

	return 0;
}

void* Profiler(void* arg)
{
	int i;
	while (1)
	{
		// CPU 점유율 & 동작 온도 & 동작 전압 -> 엑셀 파일 저장
		system("top -b -n1 | grep -Po '[0-9.]+ id' | awk '{print 100-$1}' > ./cpu_usage.csv");
		//system("sudo vcgencmd measure_temp >> /home/pi/UDP/300/cpu_temp.csv");
		//system("sudo vcgencmd measure_volts core >> /home/pi/UDP/300/cpu_volt.csv");
		// 메모리 총 용량, 사용량, 여유 -> 엑셀 파일 저장
		system("free | grep ^Mem | awk '{print $2}' >> ./mem_cap.csv");
		system("free | grep ^Mem | awk '{print $3}' >> ./mem_usage.csv");
		system("free | grep ^Mem | awk '{print $4}' >> ./mem_free.csv");
		sleep(1);
	}
}


void* Mission(void* arg)
{
	// 미션 실행 시스템 콜

	char __buff[BUFSIZE];

	//백그라운드에서 Mission Program 실행
	sprintf(__buff,
		"sudo env LD_PRELOAD=/usr/lib/aarch64-linux-gnu/liblttng-ust-cyg-profile.so "
		"sudo %s &", MSDIR);
	system(__buff);

	ftime(&milli_now);
    now = localtime(&milli_now.time);

	FILE* pFile = fopen("./failover_s.csv", "a+");
	if (Error_type == 2) {

		fprintf(pFile, "High temperature, HB_Interval = %d,Test_Count = %d, start,%d-%d-%d, %d:%d:%d.%d, %dms\n", HB_INTERVAL, Test_Count,
		now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
		(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
		Error_type = 0;
	}

	else if(Error_type == 3){
		fprintf(pFile, "Memory fault, HB_Interval = %d,Test_Count = %d, start,%d-%d-%d, %d:%d:%d.%d, %dms\n", HB_INTERVAL, Test_Count,
		now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
		(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
		Error_type = 0;
	}

	else if(Error_type == 4){
		fprintf(pFile, "voltage fault, HB_Interval = %d,Test_Count = %d, start,%d-%d-%d, %d:%d:%d.%d, %dms\n", HB_INTERVAL, Test_Count,
		now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
		(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
		Error_type = 0;
	}
	else fprintf(pFile, "Power Down, HB_Interval = %d,Test_Count = %d, start,%d-%d-%d, %d:%d:%d.%d, %dms\n", HB_INTERVAL, Test_Count,
		now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
		(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
	
	fclose(pFile);

	if (Test_Count == 10)	Test_Count = 1;
	else Test_Count++;

	return NULL;
}

void error_handling(char* message)
{
	printf("%s\n", message); // 문자열 포인터를 출력하기 위해 %s 사용
	exit(1); // 오류 발생 시 프로그램 종료
}
