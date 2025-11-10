#include <stdlib.h>
#include <stdio.h>

int main(){

    while(1){
        system("sudo ntpdate -b 210.98.16.101");
        sleep(5);
    }
}