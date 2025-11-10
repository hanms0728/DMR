#include <stdio.h>
#include <stdlib.h>

int main() {
    // 실행할 task_id 목록 정의
    int task_ids[] = {4 ,5, 6, 8, 9};
    int num_tasks = sizeof(task_ids) / sizeof(task_ids[0]);

    for (int i = 0; i < num_tasks; i++) {
        int id = task_ids[i];
        char command[256];

        // 커맨드 문자열 구성
        snprintf(command, sizeof(command),
                 "./DMR_Test_Controller 50 10 50 200 %d -p basicmath_large", id);

        printf("Running: %s\n", command);

        int ret = system(command);

        if (ret == -1) {
            perror("system() failed");
            break;
        } else {
            printf("Task with ID %d finished. Proceeding to next...\n\n", id);
        }
    }

    printf("All specified tasks finished.\n");
    return 0;
}
