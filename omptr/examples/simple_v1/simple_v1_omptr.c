#include <omp.h>
#include <stdio.h>
#include "omptr.h"

void main_task(int taskID);

int main() {
    OMPTR_INIT();

    int max_threads = 8;
    omp_set_num_threads(max_threads);

    #pragma omp parallel
    {
        #pragma omp single
        {
            OMPTR_TASK_START();
            // The main task creates and executes three subtasks
            OMPTR_BEFORE_TASK();
            #pragma omp task firstprivate(omptr_bb_id)
            {
                OMPTR_TASK_START();
                main_task(1);
                OMPTR_TASK_END();
            }
            OMPTR_AFTER_TASK();

            // Additional work before creating the next task
            printf("Additional work between task creations\n");

            OMPTR_BEFORE_TASK();
            #pragma omp task firstprivate(omptr_bb_id)
            {
                OMPTR_TASK_START();
                main_task(2);
                OMPTR_TASK_END();
            }
            OMPTR_AFTER_TASK();

            // Additional work before creating the next task
            printf("Additional work between task creations\n");

            OMPTR_BEFORE_TASK();
            #pragma omp task firstprivate(omptr_bb_id)
            {
                OMPTR_TASK_START();
                main_task(3);
                OMPTR_TASK_END();
            }
            OMPTR_AFTER_TASK();

            // Explicitly wait for all tasks to complete
            OMPTR_BEFORE_TASKWAIT();
            #pragma omp taskwait
            OMPTR_AFTER_TASKWAIT();
            OMPTR_TASK_START();
        }
    }

    OMPTR_PRINT("simple_v1.json");
    return 0;
}

void main_task(int taskID) {
    printf("Task %d is being executed\n", taskID);
}
