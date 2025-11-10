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
#include <math.h>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/stat.h>
#include <sys/reboot.h>


#define BUFSIZE 1024 // 버퍼 사이즈
#define HBSIZE 5 // Primary상태(1) + 현재 HB주기(3) + 널문자(1)
#define HBCHECK 3 // HB 수신 체크 횟수
#define THRESHOLD  62//임계온도 62도
#define RAPID_THRESHOLD 30//급격한 임계온도변화율 30도
#define VOLTAGE_THRESHOLD 4.5
#define RAPID_VOLTAGE_THRESHOLD 0.2

// 미션 프로그램 이름
char mission_name[256] = ""; // 미션 프로그램 이름을 저장할 변수 초기화

int CHECKPOINT = 0;
int HB_INTERVAL = 0;
int HB_INTERVAL_ns = 0;

typedef struct Control {
	int mission; //미션 동작 상태 (1: 미션 수행 중 -> work! 메세지를 송신)
	int active;  // 상대 시스템 동작 상태 (0: stand 수신, 1: work! 수신)
	int replacement; // 미션 대체 실행 (1: HB 미수신 -> 미션 실행)
	int available; //재부팅 가능 상태 (1: 상대 시스템 가동 중 -> 재부팅 가능)
	int reboot; //재부팅 명령 (1: 재부팅 수행)

} CT;
CT ct;

void* Heartbeat(void* arg);
void* Profiler(void* arg);
void* Mission(void* arg);
void error_handling(char* message);
pthread_mutex_t mutx;

int tmp_chk(); //현재 보드의 온도를 확인하는 함수
int tmp_rapid_chk();
float voltage_chk();


//trace를 위한 함수
void check_mission_alive(){ return ;}
void detect_mission_end(){ return ;}
void send_heartbeat() { return ;}

int heartbeat_step(
    int sockfd,
    struct sockaddr_in *clntaddr,
    socklen_t *clnt_leng,
    int HB_INTERVAL,
    volatile int stop,          // fault code (4~9 triggers alert)
    CT *ct,                    // shared context (was `ct`)
    pthread_mutex_t *mutx,      // mutex for `ct`
    int *hb_check,              // miss counter
    int *hb_state,              // 1: ok, 0: no link
    int *hb_prevent            // to prevent repeated "No HB" logs
);




int stop = 0; //발생한 고장의 종류를 저장하는 변수

//온도 고장 발생시 사용하는 변수
int time_now = 0;
int time_now_tmp = 0;
int temp_tmp = 50;
float time_now_vol = 5.0;
float temp_vol = 5.0;

int total_memory = 8123320;

struct timeb milli_now;
struct tm *now;

int Error_Type = 0;
	


int main(int argc, char* argv[])
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s arg1 arg2\n", argv[0]);
		return 1;
	}
	HB_INTERVAL = atoi(argv[1]); // HB주기 값
	Error_Type = atoi(argv[2]);//고장발생 1-> 고장 없음(전원 절체), 2-> 미션 속도 느림(WDT), 3-> 통신 두절 ,4-> 온도 고장, 5-> 급격한 온도 변화 고장, 6-> 메모리 고장, 7-> 유후 메모리 부족  ,8-> 전압 고장, 9-> 급격한 전압 변화 고장
	// -p 플래그 및 그 뒤의 미션 프로그램 이름 처리
	for (int i = 3; i < argc; i++) { // argv[1]은 이미 HB_INTERVAL로, argv[1]은 이미 Error_Type로 사용되었으므로, i는 3부터 시작
		if (strcmp(argv[i], "-p") == 0) { // -p 플래그 확인
			if (i + 1 < argc) { // -p 플래그 이후 추가 인자 확인
				strncpy(mission_name, argv[i + 1], sizeof(mission_name) - 1);
				mission_name[sizeof(mission_name) - 1] = '\0'; // 널 종료 보장
				i++; // 프로세스 이름 인자 건너뛰기
			}
			else {
				fprintf(stderr, "Error: -p flag requires a mission program name.\n");
				return 1;
			}
		}
	}

	printf("\n\nPrimary system is starting...\n");

	sleep(1);
	

	pthread_t pt_hb, pt_m, pt_p, pt_mw;
	void* pt_result;
	int mission_first = 0;

	// mutex 생성
	if (pthread_mutex_init(&mutx, NULL)) {
		error_handling("mutex init error");
	}

	// 하트비트 스레드 생성
	printf("\nCreate HB thread...\n");
	printf("HB_Interval: %d\n", HB_INTERVAL);
	pthread_create(&pt_hb, NULL, Heartbeat, NULL);

	// 프로파일러 스레드 생성
	//printf("\nCreate Profiler thread...\n");
	//pthread_create(&pt_p, NULL, Profiler, NULL);


	sleep(3);


	while (1)
	{
		// 시스템 첫 가동 후, 미션 스레드 실행
		if (0 == ct.active && 0 == mission_first) // 상대 시스템 미션 수행 X && 미션 중복 수행 X
		{
			printf("\nCreate Mission thread... (initial)\n");

			pthread_create(&pt_m, NULL, Mission, NULL);
			pthread_join(pt_m, &pt_result);

			mission_first = 1; // 중복 방지용 변수
		}
		

		// 미션 대체 실행
		if (1 == ct.replacement && 1 == ct.active) // HB 미수신 && 상대 프로그램이 미션 수행
		{
			printf("\nCreate Mission thread...\n");
			printf("\nReplacement command execute...\n");

			pthread_create(&pt_m, NULL, Mission, NULL);
			pthread_join(pt_m, &pt_result);

			mission_first = 1; //중복 방지
			//while문 loop 내에서 중간에 신호가 끊겨 아래 if문이 먼저 실행되면
			//다음 loop에서 위의 if문이 실행되어버리는 경우가 있음.
			//따라서 아래의 if문에도 중복 방지 변수를 1로 바꿔줘야 함.
		}

		//통신 두절(고장 번호: 3)
		if(Error_Type == 3 && 1 == mission_first) {

			sleep(5);

			pthread_cancel(pt_hb);	

			ftime(&milli_now);
    		now = localtime(&milli_now.time);

			stop = 3;

			FILE* pFile = fopen("./failover_p.csv", "a+"); //고장이 발생한 시간 저장
			fprintf(pFile, "heartbeat thread down, %d-%d-%d %d:%d:%d.%d, %d\n",
			now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
			(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
			fclose(pFile);

			
			printf("\n\nHeartbeat thread down[%d-%d-%d %d:%d:%d.%d]\n",
				now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm);
				printf("\n");

			// 실행 중인 미션 프로그램 kill
			char __buff[BUFSIZE];
			sprintf(__buff, "sudo pkill %s", mission_name);
			system(__buff);
			
			break;

		}

		//high temperature shutdown(고장 번호4)
		if(Error_Type == 4 && (tmp_rapid_chk() > THRESHOLD) && 1 == mission_first) {
			ftime(&milli_now);
    		now = localtime(&milli_now.time);

			stop = 4;

			FILE* pFile = fopen("./failover_p.csv", "a+"); //고장이 발생한 시간 저장
			fprintf(pFile, "high temperature shutdown, %d-%d-%d %d:%d:%d.%d, %d\n",
			now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
			(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
			fclose(pFile);

			
			printf("\n\nhHigh temperature detection[%d-%d-%d %d:%d:%d.%d]\n",
				now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm);
				printf("\n");

			// 실행 중인 미션 프로그램 kill
			char __buff[BUFSIZE];
			sprintf(__buff, "sudo pkill %s", mission_name);
			system(__buff);
			
			break;

		}

		//rapid temperature shutdown(고장 번호 5)
		if(Error_Type == 5 && 1 == mission_first) {
			ftime(&milli_now);
			now = localtime(&milli_now.time);

			if(time_now != now->tm_sec){
				temp_tmp = tmp_rapid_chk();
				if(abs(time_now_tmp - temp_tmp) > RAPID_THRESHOLD){
					stop = 5;

					FILE* pFile = fopen("./failover_p.csv", "a+"); //고장이 발생한 시간 저장
					fprintf(pFile, "rapid temperature shutdown, %d-%d-%d %d:%d:%d.%d, %d\n",
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
					(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
					fclose(pFile);

					printf("\n\nRapid temperature change detection[%d-%d-%d %d:%d:%d.%d]\n",
						now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm);
						printf("\n");

					// 실행 중인 미션 프로그램 kill
					char __buff[BUFSIZE];
					sprintf(__buff, "sudo pkill %s", mission_name);
					system(__buff);
					
					break;
				}
				time_now = now->tm_sec;
				time_now_tmp = temp_tmp;
			}

		}

		//memort fault(고장번호 6)
		if(Error_Type == 6 && 1 == mission_first){

			check_mission_alive();
			
			char buffer[1024];
			char MissionCommand[1024];
			
			sprintf(MissionCommand, "ps -ef | grep ./%s | grep -v grep ", mission_name); // ./ 빼면 문제 생김
			FILE* fp = popen(MissionCommand, "r");
			if (fp == NULL) {
				perror("popen");
				return 1;
			}
			
			char* Mission_Program_PID; //미션 프로그램의 PID
			Mission_Program_PID = fgets(buffer, 1024, fp);
		      
			if (Mission_Program_PID == NULL) { //mission이 동작중이지 않으면
				detect_mission_end();

				ftime(&milli_now);
    			now = localtime(&milli_now.time);

				stop = 6;

				FILE* pFile = fopen("./failover_p.csv", "a+"); //고장이 발생한 시간 저장
				fprintf(pFile, "Memory fault shutdown, %d-%d-%d %d:%d:%d.%d, %d\n",
				now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
				(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
				fclose(pFile);
				pclose(fp);
				
				printf("Mission operation stoppage detection \n[%d-%d-%d %d:%d:%d.%d]\n",
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm);
					printf("\n");

				char __buff[BUFSIZE];
				sprintf(__buff, "sudo pkill %s", mission_name);
				system(__buff);
				
				break;
			}
			
			pclose(fp);

		}

		//low memory shutdown(고장 번호 7)
		if(Error_Type == 7 && 1 == mission_first) {
			char buffer[1024];
			
			FILE* fp = popen("free | grep Mem | awk '{print $3}'", "r");
			if (fp == NULL) {
				perror("popen");
				return 1;
			}
			
			int free_memory; //미션 프로그램의 PID
			free_memory = atoi(fgets(buffer, 1024, fp));

			pclose(fp);

			if(free_memory < total_memory/7){
				ftime(&milli_now);
				now = localtime(&milli_now.time);

				stop = 7;

				FILE* pFile = fopen("./failover_p.csv", "a+"); //고장이 발생한 시간 저장
				fprintf(pFile, "Low memory shutdown, %d-%d-%d %d:%d:%d.%d, %d\n",
				now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
				(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
				fclose(pFile);
				
				printf("Memory shortage occurrence detection[%d-%d-%d %d:%d:%d.%d]\n",
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm);
				printf("\n");
				printf("\n");
				
				break;
			}
		}

		//low voltage shutdown(고장 번호 8)
		if(Error_Type == 8 && (voltage_chk() < VOLTAGE_THRESHOLD) && 1 == mission_first) {
			ftime(&milli_now);
    		now = localtime(&milli_now.time);

			stop = 8;

			FILE* pFile = fopen("./failover_p.csv", "a+"); //고장이 발생한 시간 저장
			fprintf(pFile, "low voltage shutdown, %d-%d-%d %d:%d:%d.%d, %d\n",
			now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
			(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
			fclose(pFile);

			
			printf("\n\nLow voltage detection[%d-%d-%d %d:%d:%d.%d]\n",
				now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm);
				printf("\n");

			// 실행 중인 미션 프로그램 kill
			char __buff[BUFSIZE];
			sprintf(__buff, "sudo pkill %s", mission_name);
			system(__buff);
			
			break;

		}

		//rapid voltage shutdown(고장 번호 9)
		if(Error_Type == 9 && 1 == mission_first) {
			ftime(&milli_now);
			now = localtime(&milli_now.time);

			if(time_now != now->tm_sec){
				temp_vol = voltage_chk();
				if(fabsf(time_now_vol - temp_vol) > RAPID_VOLTAGE_THRESHOLD){
					stop = 9;

					FILE* pFile = fopen("./failover_p.csv", "a+"); //고장이 발생한 시간 저장
					fprintf(pFile, "rapid voltage shutdown, %d-%d-%d %d:%d:%d.%d, %d\n",
					now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
					(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
					fclose(pFile);

					printf("\n\nRapid voltage detection[%d-%d-%d %d:%d:%d.%d]\n",
						now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm);
						printf("\n");

					// 실행 중인 미션 프로그램 kill
					char __buff[BUFSIZE];
					sprintf(__buff, "sudo pkill %s", mission_name);
					system(__buff);
					
					break;
				}
				time_now = now->tm_sec;
				time_now_vol = temp_vol;
			}

		}

		if (1 == ct.reboot && 1 == ct.available)
		{
			printf("\n\nRebooting...\n");

			// 실행 중인 미션 프로그램 kill
			char __buff[BUFSIZE];
			sprintf(__buff, "sudo pkill %s", mission_name);
			system(__buff);
		}
		
		
	}

	pthread_join(pt_hb, &pt_result);

	printf("\nPrimary system is shut down...\n");

	return 0;
}

void* Heartbeat(void* arg)
{
	int hb_check; // HB 미수신 3회 체크
	int hb_prevent = 0; // 미션 중복 실행 방지용
	int hb_active; // 상대 시스템의 미션 수행 상태
	int hb_state; // HB 통신 연결 상태

	char snd_buff[HBSIZE]; // HB 송신 버퍼
	char rcv_buff[HBSIZE]; // HB 수신 버퍼


	// 소켓 생성 변수
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
	read_timeout.tv_usec = HB_INTERVAL_ns;

	// 소켓 송수신 타임아웃 설정(Non-Blocking)
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout); //rcv 제한 시간
	//setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &read_timeout, sizeof read_timeout);

	// 소켓 기본 설정
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; // IPv4 
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(usingPort);

	// 소켓 바인딩
	if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
		error_handling("UDP bind() error");
	printf("\nBinding is done...\n");

	while (1) {
		int should_break = heartbeat_step(
		sockfd,
		&clntaddr,
		&clnt_leng,
		HB_INTERVAL,
		stop,
		&ct,
		&mutx,
		&hb_check,
		&hb_state,
		&hb_prevent
		);
		if (should_break) break;

		usleep(HB_INTERVAL_ns); // HB 간격만큼 딜레이
		//break; //테스트
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
		//system("sudo vcgencmd measure_temp >> ./cpu_temp.csv");
		//system("sudo vcgencmd measure_volts core >> ./cpu_volt.csv");
		// 메모리 총 용량, 사용량, 여유 -> 엑셀 파일 저장
		system("free | grep ^Mem | awk '{print $2}' >> ./mem_cap.csv");
		system("free | grep ^Mem | awk '{print $3}' >> ./mem_usage.csv");
		system("free | grep ^Mem | awk '{print $4}' >> ./mem_free.csv");
		sleep(1);

	}
}

void* Mission(void* arg)
{
	pthread_mutex_lock(&mutx);
	ct.mission = 1;  // 1: 본 시스템이 미션 실행 (work! 수신으로 변경)
	pthread_mutex_unlock(&mutx);

	pthread_mutex_lock(&mutx);
	ct.replacement = 0; // 미션을 대체했으므로, 미션 대체 실행 변수 초기화
	pthread_mutex_unlock(&mutx);

	char __buff[BUFSIZE];
	sprintf(__buff,
		 "sudo env LD_PRELOAD=/usr/lib/aarch64-linux-gnu/liblttng-ust-cyg-profile.so "
		 "./%s %d &", mission_name, Error_Type);
	//sprintf(__buff, "gnome-terminal -- bash -c \"sudo ./%s %d; exec bash\"",mission_name, Error_Type);
	system(__buff);

	printf("mission is running....\n");

	return 0;
}

void error_handling(char* message)
{
	fputs(message, stderr);
	printf("\n");
	fprintf(stderr, "error: %s\n", strerror(errno));
	exit(1);
}


// function for temperature check
//sudo apt install lm-sensors 필요
int tmp_chk(){
	//아래는 ubuntu에서 실행시 필요
	/*system("sensors | grep 'temp' | awk '{print $2}' > ./temp.csv");
	
	FILE* tmp_log;
	char str[5];
	int temp;

	tmp_log = fopen("temp.csv", "r");
	temp = atoi(fgets(str, 5, tmp_log));
	fclose(tmp_log);

	return temp;
	*/

	//아래는 라지비안 에서 동작
	FILE* tmp_log;
	char str[8];
	int temp;
	tmp_log = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
	temp = atoi(fgets(str, 7, tmp_log));
	fclose(tmp_log);
	return temp;
}
////////////////////////////////

int tmp_rapid_chk(){

	FILE* tmp_log;
	char str[50];
	int temp;
	tmp_log = fopen("temp.csv", "r");
	temp = atoi(fgets(str, 5, tmp_log));
	fclose(tmp_log);
	return temp;
}

float voltage_chk(){

	FILE* vol_log;
	char str[10];
	float vol;
	vol_log = fopen("voltage.csv", "r");
	vol = atof(fgets(str, 5, vol_log));
	fclose(vol_log);
	return vol;
}


// Returns 1 if the caller should break the while-loop (alert/stop case), else 0
int heartbeat_step(
    int sockfd,
    struct sockaddr_in *clntaddr,
    socklen_t *clnt_leng,
    int HB_INTERVAL,
    volatile int stop,          // fault code (4~9 triggers alert)
    CT *ct,                    // shared context (was `ct`)
    pthread_mutex_t *mutx,      // mutex for `ct`
    int *hb_check,              // miss counter
    int *hb_state,              // 1: ok, 0: no link
    int *hb_prevent            // to prevent repeated "No HB" logs
){
    // 1) Buffers
    char snd_buff[1024];
    char rcv_buff[1024];
    memset(snd_buff, 0, HBSIZE);
    memset(rcv_buff, 0, HBSIZE);

    // 2) Build TX payload: status + HB interval (keep your "0" prefix rule)
    char status = 'a'; // default: alive
    if (ct->mission == 1)            status = 'w';
    if (stop == 4 || stop == 5)      status = 'h';
    else if (stop == 6 || stop == 7) status = 'm';
    else if (stop == 8 || stop == 9) status = 'v';

    if (HB_INTERVAL >= 100)
        snprintf(snd_buff, HBSIZE, "%c%d", status, HB_INTERVAL);
    else
        snprintf(snd_buff, HBSIZE, "%c0%d", status, HB_INTERVAL);

    // 3) Receive (up to 3 misses counted)
    int n = 0;
    socklen_t len = *clnt_leng;
    while ((n = recvfrom(sockfd, rcv_buff, (int)HBSIZE, 0,
                         (struct sockaddr*)clntaddr, &len)) <= 0
           && *hb_check < 3) {
        (*hb_check)++;
    }
    *clnt_leng = len;

    // 4) Evaluate HB state
    if (*hb_check >= 3) {
        *hb_state = 0; // link down
    }
    if (n > 0) {
        *hb_state = 1;
        *hb_check = 0;

        // peer says "alive"
        if (rcv_buff[0] == 'a') {
            pthread_mutex_lock(mutx);
            ct->active      = 0;
            ct->available   = 1;
            ct->replacement = 0;
            pthread_mutex_unlock(mutx);
        }
        // peer says "work!"
        else if (rcv_buff[0] == 'w') {
            pthread_mutex_lock(mutx);
            ct->active      = 1;
            ct->available   = 1;
            ct->replacement = 0;
            pthread_mutex_unlock(mutx);
        }
    }

    // 5) No HB link: mark replacement once
    if (*hb_state == 0) {
        if (*hb_prevent == 0) {
            pthread_mutex_lock(mutx);
            ct->replacement = 1; // need replacement
            ct->available   = 0; // peer is down
            pthread_mutex_unlock(mutx);

            *hb_prevent = 1;
            printf("\n(HB thread) No HB Signals\n");
        }
    }

    // 6) Transmit HB
    send_heartbeat();
    sendto(sockfd, snd_buff, (int)HBSIZE, 0, (struct sockaddr*)clntaddr, *clnt_leng);

    // 7) Alert-stop condition (4~9): log timestamp and request break
    if (stop == 4 || stop == 5 || stop == 6 || stop == 7 || stop == 8 || stop == 9) {
        struct timeb milli_now; ftime(&milli_now);
        struct tm *now = localtime(&milli_now.time);
        printf("Transmission of Alert signal to secondary board completed\n"
               "[%d-%d-%d %d:%d:%d.%d]\n\n",
               now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
               now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm);
        return 1; // tell caller to break the while-loop
    }

    return 0;
}