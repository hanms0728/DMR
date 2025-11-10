#include "snipmath.h"
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include <errno.h>

int fd;
int timeout = 30;  // Watchdog 타임아웃 시간 (초 단위)
int ret;
int count = 0;

void Inject_memory_failure() {
    volatile int* p = (int*)0x12345678;
    *p = 1;  // 고의적인 메모리 오류
} //메모리 고장을 발생시키는 함수

/* The printf's may be removed to isolate just the math calculations */

int main(int argc, char* argv[])
{
  

   if(argc != 2){
	fprintf(stderr, "Usage: %s arg1 \n", argv[0]);
	return 1;
  	}
	int Error_Type = atoi(argv[1]);


	//고장 번호가 2번일 경우 watchdog timer 설정
	if(Error_Type == 2){
		// /dev/watchdog 디바이스 열기
		fd = open("/dev/watchdog", O_WRONLY);
		if (fd == -1) {
			fprintf(stderr, "Failed to open /dev/watchdog: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}

		// Watchdog 타임아웃 설정
		ret = ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
		if (ret == -1) {
			fprintf(stderr, "Failed to set timeout: %s\n", strerror(errno));
			close(fd);
			return EXIT_FAILURE;
		}

		ret = write(fd, "\0", 1);

	}
	
  while(1){
	  double  a1 = 1.0, b1 = -10.5, c1 = 32.0, d1 = -30.0;
	  double  x[3];
	  double X;
	  int     solutions;
	  int i;
	  unsigned long l = 0x3fed0169L;
	  struct int_sqrt q;
	  long n = 0;

	  /* solve soem cubic functions */
	  
	  printf("********* CUBIC FUNCTIONS ***********\n");
	  /* should get 3 solutions: 2, 6 & 2.5   */
	  
	  SolveCubic(a1, b1, c1, d1, &solutions, x);  
	  printf("Solutions:");
	  for(i=0;i<solutions;i++)
	    printf(" %f",x[i]);
	  printf("\n");

	  a1 = 1.0; b1 = -4.5; c1 = 17.0; d1 = -30.0;
	  /* should get 1 solution: 2.5           */
	  
	  SolveCubic(a1, b1, c1, d1, &solutions, x);  
	  printf("Solutions:");
	  for(i=0;i<solutions;i++)
	    printf(" %f",x[i]);
	  printf("\n");

	  a1 = 1.0; b1 = -3.5; c1 = 22.0; d1 = -31.0;
	  
	  SolveCubic(a1, b1, c1, d1, &solutions, x);
	  printf("Solutions:");
	  for(i=0;i<solutions;i++)
	    printf(" %f",x[i]);
	  printf("\n");

	  a1 = 1.0; b1 = -13.7; c1 = 1.0; d1 = -35.0;
	  
	  SolveCubic(a1, b1, c1, d1, &solutions, x);
	  //printf("Solutions:");
	  for(i=0;i<solutions;i++)
	    //printf(" %f",x[i]);
	  //printf("\n");

	  a1 = 3.0; b1 = 12.34; c1 = 5.0; d1 = 12.0;
	  
	  SolveCubic(a1, b1, c1, d1, &solutions, x);
	  printf("Solutions:");
	  for(i=0;i<solutions;i++)
	    printf(" %f",x[i]);
	  printf("\n");

	  a1 = -8.0; b1 = -67.89; c1 = 6.0; d1 = -23.6;
	  
	  SolveCubic(a1, b1, c1, d1, &solutions, x);
	  printf("Solutions:");
	  for(i=0;i<solutions;i++)
	    printf(" %f",x[i]);
	  printf("\n");

	  a1 = 45.0; b1 = 8.67; c1 = 7.5; d1 = 34.0;
	  
	  SolveCubic(a1, b1, c1, d1, &solutions, x);
	  printf("Solutions:");
	  for(i=0;i<solutions;i++)
	    printf(" %f",x[i]);
	  printf("\n");

	  a1 = -12.0; b1 = -1.7; c1 = 5.3; d1 = 16.0;
	  
	  SolveCubic(a1, b1, c1, d1, &solutions, x);
	  printf("Solutions:");
	  for(i=0;i<solutions;i++)
	    printf(" %f",x[i]);
	  printf("\n");

	  /* Now solve some random equations */
	  
	  for(a1=1;a1<10;a1+=1) {
	    for(b1=10;b1>0;b1-=.25) {
	      for(c1=5;c1<15;c1+=0.61) {
		   for(d1=-1;d1>-5;d1-=.451) {
			SolveCubic(a1, b1, c1, d1, &solutions, x);  
			printf("Solutions:");
			for(i=0;i<solutions;i++)
			  printf(" %f",x[i]);
			printf("\n");
		   }
	      }
	    }
	  }

	  //메모리 고장을 일으키는 코드
	  if(Error_Type == 6 && count > 3) Inject_memory_failure();


	  printf("********* INTEGER SQR ROOTS ***********\n");
	  /* perform some integer square roots */
	  
	  for (i = 0; i < 100000; i+=2)
	    {
	      usqrt(i, &q);
				// remainder differs on some machines
	      //printf("sqrt(%3d) = %2d, remainder = %2d\n",
	      printf("sqrt(%3d) = %2d\n",
		     i, q.sqrt);
	    }
	  printf("\n");
	  
	  for (l = 0x3fed0169L; l < 0x3fed4169L; l++)
	    {
		 usqrt(l, &q);
		 //printf("\nsqrt(%lX) = %X, remainder = %X\n", l, q.sqrt, q.frac);
		 printf("sqrt(%lX) = %X\n", l, q.sqrt);
	    }

	  printf("********* ANGLE CONVERSION ***********\n");
	  /* convert some rads to degrees */
	/*   for (X = 0.0; X <= 360.0; X += 1.0) */
	
	  for (X = 0.0; X <= 360.0; X += .001){}
	    printf("%3.0f degrees = %.12f radians\n", X, deg2rad(X));
	  puts("");
	/*   for (X = 0.0; X <= (2 * PI + 1e-6); X += (PI / 180)) */
	
	  for (X = 0.0; X <= (2 * PI + 1e-6); X += (PI / 5760)){}
	    printf("%.12f radians = %3.0f degrees\n", X, rad2deg(X));

	count++;
  }
}