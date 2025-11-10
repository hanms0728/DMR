#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>


#include <time.h>
#include <sys/timeb.h>
    

#define MAX_BUFFER_SIZE 1024

pid_t pid;
char buffer[MAX_BUFFER_SIZE];
char command[256];
int num_pid;
int MPRun = 0;
int Error_Type1_Active;
int Error_Type2_Active;
int Error_Type3_Active;
int Error_Type4_Active;
int Error_Type5_Active;
int Error_Type6_Active;
int Error_Type7_Active;
int Error_Type8_Active;
int Error_Type9_Active;
char mission_name[256] = ""; // 기본 프로세스 이름

void* Primary_Kill_thread(void* arg);

struct timeb milli_now;
struct tm *now;	
int time_now = 0;
int time_count = 0;
float voltage = 5.0;

int main(int argc, char* argv[]) {
	if (argc < 6) {
		printf("Usage: %s arg1 arg2 arg3 arg4 arg5 [-p mission_program_name]\n", argv[0]);
		return 1;
	}

	// 플래그 처리
	int HB_Start = atoi(argv[1]); // 시작 HB 값
	int Test_Count = atoi(argv[2]); // HB당  실험 횟수 값
	int HB_Interval = atoi(argv[3]); // HB 주기 증가 값
	int HB_End = atoi(argv[4]); // 종료 HB 값
	int Error_Type = atoi(argv[5]); // 고장 유형, 1-> 고장 없음(전원 절체), 2-> 미션 속도 느림(WDT), 3-> 통신 두절 ,4-> 온도 고장, 5-> 급격한 온도 변화 고장, 6-> 메모리 고장,7-> 유후 메모리 부족  ,8-> 전압 고장, 9-> 급격한 전압 변화 고장


	for (int i = 6; i < argc; i++) {
		if (strcmp(argv[i], "-p") == 0) { // -p 플래그 확인
			if (i + 1 < argc) { // -p 플래그 이후 추가 인자 확인
				strncpy(mission_name, argv[i + 1], sizeof(mission_name) - 1);
				mission_name[sizeof(mission_name) - 1] = '\0'; // 널 종료 보장
				i++; // 프로세스 이름 인자 건너뛰기
			}
			else {
				printf("Error: -p flag requires a mission program name.\n");
				return 1;
			}
		}
	}

	for (int i = HB_Start; i <= HB_End; i += HB_Interval)
	{
		for (int j = 1; j <= Test_Count; j++) {
			// DMR Test Controller 프로그램 실행을 위한 자식 프로세스 생성
			// 프로세스 : 어떤 프로그램을 실행하기 위한 자원 모음/공간
			pid = fork();

			if (pid == 0) {
				// 자식 프로세스에서 Primary 프로그램 실행
				// 백그라운드로 Primary 프로그램이 실행되더라도 표준 출력(STDOUT: Printf)은 보이는게 정상임
				printf("\n");
				
				char PrimaryCommand[MAX_BUFFER_SIZE];
				printf("Current Mission: %s\n", mission_name);
				printf("Current HB: %d\n", i);
				printf("Current HB - Test Count: %d\n", j);
				sprintf(PrimaryCommand, "sudo ./Primary %d %d -p %s &", i, Error_Type ,mission_name);
				// sprintf(PrimaryCommand, "./Primary %d > /dev/null 2>&1 &", i); // Primary와 Mission Program의 출력이 안 보이도록 하는 코드
				system(PrimaryCommand);

				exit(0);
			}
			else if (pid > 0) {
				Error_Type1_Active = 0;
				Error_Type2_Active = 0;
				Error_Type3_Active = 0;
				Error_Type4_Active = 0;
				Error_Type5_Active = 0;
				Error_Type6_Active = 0;
				Error_Type7_Active = 0;
				Error_Type8_Active = 0;

				while (1) {
					// 미션 프로그램 PID 검색
					char MissionCommand[MAX_BUFFER_SIZE];
					sprintf(MissionCommand, "ps -ef | grep ./%s | grep -v grep | grep -v sudo | awk '{print $2}'", mission_name); // ./ 빼면 문제 생김
					FILE* fp = popen(MissionCommand, "r");
					if (fp == NULL) {
						perror("popen");
						return 1;
					}


					char* Mission_Program_PID;
					Mission_Program_PID = fgets(buffer, MAX_BUFFER_SIZE, fp);

					pthread_t tid;
					// 아래 if 문은 전원 절체 고장, 온도 고장일 경우에 작동함
					// 미션 프로그램 PID가 검색된 경우
					if (Mission_Program_PID != NULL) {
						if (sscanf(buffer, "%d", &num_pid) == 1) {
							MPRun = 1;

							// 전원 절체 고장
							if (Error_Type == 1 && Error_Type1_Active == 0) {
								printf("Error Type 1: Power Down\n");
								printf("The automation system automatically ends the mission program.\n");
								//pthread_t tid;
								pthread_create(&tid, NULL, Primary_Kill_thread, NULL);
								Error_Type1_Active = 1;
							}
							
							//WDT
							if (Error_Type == 2 && Error_Type2_Active == 0) {
								printf("Error Type 2: Slow mission running\n");
								printf("The automation system automatically ends the mission program.\n");
								Error_Type2_Active = 1;
							}

							//통신 두절
							if (Error_Type == 3 && Error_Type3_Active == 0) {
								printf("Error Type 3: Heartbeat process down\n");
								printf("Heartbeat process down\n");
								Error_Type3_Active = 1;
							}

							// 온도 고장
							if (Error_Type == 4 && Error_Type4_Active == 0) {
								printf("Error Type 4: CPU Temp High\n");
								printf("The DMR system automatically detects the temperature and terminates the mission program.\n");
								Error_Type4_Active = 1;
							}

							//급격한 온도 고장
							if (Error_Type == 5 && Error_Type5_Active == 0) {
								printf("Error Type 5: CPU Temp rapid change\n");
								printf("The DMR system automatically detects the rapid temperature change and terminates the mission program.\n");
								Error_Type5_Active = 1;
							}

							// 메모리 고장
							if (Error_Type == 6 && Error_Type6_Active == 0) {
								printf("Error Type 6: Memory Error\n");
								printf("The automation system automatically ends the mission program.\n");
								Error_Type6_Active = 1;
							}
							
							//유휴 메모리 부족
							if (Error_Type == 7 && Error_Type7_Active == 0) {
								printf("Error Type 7: low memory\n");
								printf("low memory\n");
								Error_Type7_Active = 1;
							}

							//전압 부족
							if (Error_Type == 8 && Error_Type8_Active == 0) {
								printf("Error Type 8: Voltage low\n");
								printf("Voltage low\n");
								Error_Type8_Active = 1;
								voltage = 5.0;
								time_count = 0;
							}
							//급격한 전압 변화
							if (Error_Type == 9 && Error_Type9_Active == 0) {
								printf("Error Type 9: Voltage rapid change\n");
								printf("Voltage rapid change\n");
								Error_Type9_Active = 1;
							}

						}
					}

					if (Mission_Program_PID == NULL) { // 미션 프로그램 PID가 검색되지 않은 경우
						if (MPRun == 1) { // 이전에 미션 프로그램이 실행된 경우
							if(Error_Type >= 4){
								printf("Mission Program is Dead.\n");
								sleep(1);

								system("sudo pkill Primary");
								waitpid(pid, NULL, 0);
								
								printf("\n");
								pclose(fp); // 파일 닫기

								if(Error_Type5_Active == 1 || Error_Type4_Active == 1){
									FILE* pFile = fopen("temp.csv", "w");
									fprintf(pFile, "30\n");
									fclose(pFile);

									time_count = 0;
								}

								if(Error_Type7_Active == 1){
									system("sudo pkill stress-ng");
								}

								if(Error_Type8_Active == 1 || Error_Type9_Active == 1){
									FILE* pFile = fopen("voltage.csv", "w");
									fprintf(pFile, "5.0\n");
									fclose(pFile);

									time_count = 0;
								}
								break;
							}
						}
					}

					// 아래 if문은 전원절체 고장일 때 작동함
					if (Error_Type == 1 && MPRun == 1) {

						pthread_join(tid,NULL);

						printf("Primary Program is Dead.\n");

						// csv 파일 열기, 없으면 새로 생성
						FILE* pFile = fopen("./Primary_Error_Time.csv", "a+");
						fprintf(pFile, "HB_Interval = %d,Test_Count = %d,Power down, end,%d-%d-%d %d:%d:%d.%d, %dms\n", i, j,
						now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec, milli_now.millitm,
						(now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) * 1000 + milli_now.millitm);
						fclose(pFile);
						
						char commend[1024];
						sprintf(commend, "sudo pkill %s", mission_name);
						system(commend);
						
						waitpid(pid, NULL, 0);
						pclose(fp); // 파일 닫기

						break;
					}

					// 아래 if문은 온도 고장일 때 작동함
					if (Error_Type == 4 && MPRun == 1) {

						ftime(&milli_now);
						now = localtime(&milli_now.time);

						if(time_now != now->tm_sec){
							time_count++;
							FILE* pFile = fopen("temp_tmp.csv", "w");
							if(time_count < 10) fprintf(pFile, "50\n");
							else fprintf(pFile, "70\n");
							fflush(pFile);
							fclose(pFile);
							rename("temp_tmp.csv", "temp.csv");
							time_now = now->tm_sec;
						}
					}

					// 아래 if문은 급격한 온도 변화 고장일 때 작동함
					if (Error_Type == 5 && MPRun == 1) {

						ftime(&milli_now);
						now = localtime(&milli_now.time);

						if(time_now != now->tm_sec){
							time_count++;
							FILE* pFile = fopen("temp_tmp.csv", "w");
							if(time_count < 10) fprintf(pFile, "30\n");
							else fprintf(pFile, "61\n");
							fflush(pFile);
							fclose(pFile);
							rename("temp_tmp.csv", "temp.csv");
							time_now = now->tm_sec;
						}
					}

					// 아래 if문은 메모리 부족 고장일 때 작동함
					if (Error_Type == 7 && MPRun == 1) {
						sleep(5);
						system("stress-ng --vm 1 --vm-bytes 70%  &");

					}


					// 아래 if문은 전압 고장일 때 작동함
					if (Error_Type == 8 && MPRun == 1) {

						ftime(&milli_now);
						now = localtime(&milli_now.time);

						if(time_now != now->tm_sec){
							FILE* pFile = fopen("voltage_tmp.csv", "w");

							time_count++;
							if(time_count % 2 == 0) voltage -= 0.1;
							fprintf(pFile, "%.1f\n", voltage);
							fflush(pFile);
							fclose(pFile);
							rename("voltage_tmp.csv", "voltage.csv");
							time_now = now->tm_sec;
						}
					}



					// 아래 if문은 급격한 전압 변화 고장일 때 작동함
					if (Error_Type == 9 && MPRun == 1) {

						ftime(&milli_now);
						now = localtime(&milli_now.time);

						if(time_now != now->tm_sec){
							FILE* pFile = fopen("voltage_tmp.csv", "w");

							time_count++;
							if(time_count < 10) fprintf(pFile, "5.0\n");
							else fprintf(pFile, "4.7\n");
							fflush(pFile);
							fclose(pFile);
							rename("voltage_tmp.csv", "voltage.csv");
							time_now = now->tm_sec;
						}
					}

					pclose(fp); // 파일 닫기
				}
			}
			else {
				// fork() 실패 처리
				fprintf(stderr, "fork() failed.\n");
				return 1;
			}

			MPRun = 0;

			printf("\n");

			if(Error_Type == 2) sleep(30); //온도 고장의 경우 열을 식히는 시간이 필요하므로 sleep시간을 늘림
			else sleep(10);
		}
	}
	return 0;
}

void* Primary_Kill_thread(void* arg) {
	sleep(10);
	char KillCommand[MAX_BUFFER_SIZE];
	sprintf(KillCommand, "sudo pkill Primary");
	system(KillCommand);

	ftime(&milli_now);
	now = localtime(&milli_now.time);

	return NULL;
}