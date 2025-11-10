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
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/stat.h>
#include <sys/reboot.h>


#define BUFSIZE 1024 // 버퍼 사이즈
#define HBSIZE 5 // Primary상태(1) + 현재 HB주기(3) + 널문자(1)
#define HBCHECK 3 // HB 수신 체크 횟수
#define THRESHOLD  87//임계온도 62도

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

int stop = 0;

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
	Error_Type = atoi(argv[2]);//고장발생 1-> 고장 없음(전원 절체), 2-> 온도 고장, 3-> 메모리 고장
	HB_INTERVAL_ns = HB_INTERVAL * 1000;
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

		//high temperature shutdown
		if(Error_Type == 2 && (tmp_chk() > THRESHOLD) && 1 == mission_first) {
			ftime(&milli_now);
    		now = localtime(&milli_now.time);

			stop = 2;

			FILE* pFile = fopen("./failover_p.csv", "a+"); //고장이 발생한 시간 저장
			fprintf(pFile, "high temperature shutdown, %d-%d-%d %d:%d:%d.%d, %d\n",
			now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
			(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
			fclose(pFile);

			
			printf("\n\nhigh temperature shutdown\n");

			// 실행 중인 미션 프로그램 kill
			char __buff[BUFSIZE];
			sprintf(__buff, "sudo pkill %s", mission_name);
			system(__buff);
			
			break;

		} 

		//memort fault
		if(Error_Type == 3 && 1 == mission_first){
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
				ftime(&milli_now);
    			now = localtime(&milli_now.time);

				stop = 3;

				FILE* pFile = fopen("./failover_p.csv", "a+"); //고장이 발생한 시간 저장
				fprintf(pFile, "Memory fault shutdown, %d-%d-%d %d:%d:%d.%d, %d\n",
				now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
				(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
				fclose(pFile);
				pclose(fp);
				
				printf("mission is not running\n");
				printf("\n");
				
				break;
			}
			
			pclose(fp);

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
	pthread_join(pt_p, &pt_result);

	printf("\nPrimary system is shut down...\n");

	return 0;
}

void* Heartbeat(void* arg)
{
	int hb_check; // HB 미수신 3회 체크
	int hb_prevent = 0; // 미션 중복 실행 방지용
	int hb_active; // 상대 시스템의 미션 수행 상태
	int hb_state; // HB 통신 연결 상태

	int fd;
	int n;

	char snd_buff[HBSIZE]; // HB 송신 버퍼
	char rcv_buff[HBSIZE]; // HB 수신 버퍼

	// 시리얼 포트 열기
    int serial_port = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);

    if (serial_port == -1) {
        perror("Unable to open /dev/ttyAMA0");
        return 1;
    }

    // 포트 설정
    struct termios options;
    tcgetattr(serial_port, &options);

    // 보드레이트를 115200bps로 설정
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    options.c_cflag &= ~PARENB; // No parity
    options.c_cflag &= ~CSTOPB; // 1 stop bit
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8; // 8 data bits

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    tcsetattr(serial_port, TCSANOW, &options);

	
	while (1) {
		// HB 버퍼 메모리 선언
		memset(&snd_buff, 0, sizeof(char) * HBSIZE);
		memset(&rcv_buff, 0, sizeof(char) * HBSIZE);


		// 기본 상태는 "alive" 송신, 본 시스템에서 미션 수행시 "work!" 송신
		//온도 고장 발생시 "heat!" 송신
		//메모리 고장 발생시 "stop!" 송신
		//alive와 work와 heat와 stop와 더불어 현제 primary보드의 HB주기도 같이 송신
		// HB_INTERVAL이 1000미만일 경우 0을 추가하여 문자열 포맷 유지


		if (HB_INTERVAL >= 100) {
			sprintf(snd_buff, "a%d", HB_INTERVAL);
	
			if (1 == ct.mission) {
				sprintf(snd_buff, "w%d", HB_INTERVAL);
			}
			
			if(stop == 2){
				sprintf(snd_buff, "h%d",HB_INTERVAL);
			}

			if(stop == 3){
				sprintf(snd_buff, "s%d",HB_INTERVAL);
			}

		}
		
		else {
			sprintf(snd_buff, "a%d%d", 0,HB_INTERVAL);
	
			if (1 == ct.mission) {
				sprintf(snd_buff, "w%d%d", 0,HB_INTERVAL);
			}
			
			if(stop == 2){
				sprintf(snd_buff, "h%d%d",0,HB_INTERVAL);
			}

			if(stop == 3){
				sprintf(snd_buff, "s%d%d",0,HB_INTERVAL);
			}

		}

		tcflush(serial_port, TCOFLUSH);
		
		write(serial_port, snd_buff, strlen(snd_buff));// HB 송신
		//tcflush(serial_port, TCOFLUSH);


		// 시리얼에 데이터가 수신되었을 때
	/*	if(0 < read(serial_port, rcv_buff, HBSIZE - 1))	{ // HB 수신 체크
			hb_state = 1; // 1: HB 통신 연결 양호
			hb_check = 0; // int: HB 미수신 3회 체크용 변수

			rcv_buff[HBSIZE] = '\0';

			// "alive" 수신
			if (rcv_buff[0] == 'a')
			{
				pthread_mutex_lock(&mutx);
				ct.active = 0; // 0: 상대 시스템이 미션 프로그램 실행 X
				ct.available = 1; // 1: 상대 시스템이 가동 중 -> 재부팅 가능한 상태
				ct.replacement = 0; // 0: 미션 대체 실행 X
				pthread_mutex_unlock(&mutx);
			}

			// "work!" 수신
			if (rcv_buff[0] == 'w')
			{
				pthread_mutex_lock(&mutx);
				ct.active = 1; // 1: 상대 시스템이 미션 프로그램 실행 중
				ct.available = 1; // 1: 상대 시스템이 가동 중 -> 재부팅 가능한 상태
				ct.replacement = 0; // 0: 미션 대체 실행 X
				pthread_mutex_unlock(&mutx);
			}
		}
		else // 시리얼에 수신되는 데이터가 없음
		{
			if (++hb_check >= HBCHECK) //HB 수신 3회 체크
			{
				hb_state = 0; // 0: HB 통신 연결 불가
			}
		}
		
		// HB 통신 불가능
		if (0 == hb_state) // HB 신호 미수신
		{

			if (0 == hb_prevent) { // 아래 코드 반복 실행을 방지하는 용도
				pthread_mutex_lock(&mutx);
				ct.replacement = 1; // 1: 미션 대체 실행 필요
				ct.available = 0; // 0: 상대 시스템이 다운됨
				pthread_mutex_unlock(&mutx);

				hb_prevent = 1; // 반복 실행 방지

				printf("\n(HB thread) No HB Signals\n");
				//printf("No HB Signals!!\nNo need Replacement!!\n");
			}
		}
		
		tcflush(serial_port, TCIFLUSH);
		*/

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
	sprintf(__buff, "sudo ./%s %d &", mission_name, Error_Type);
	system(__buff);


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
	char str[3];
	int temp;
	tmp_log = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
	temp = atoi(fgets(str, 3, tmp_log));
	fclose(tmp_log);
	return temp;

}
////////////////////////////////
