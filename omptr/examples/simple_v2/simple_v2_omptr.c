#include <omp.h>
#include <stdio.h>
#include "omptr.h"

void main_task(int taskID, int *omptr_bb_id_ptr);

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
            main_task(1, &omptr_bb_id);

            main_task(2, &omptr_bb_id);

            main_task(3, &omptr_bb_id);

            // Explicitly wait for all tasks to complete
            OMPTR_BEFORE_TASKWAIT();
            #pragma omp taskwait
            OMPTR_AFTER_TASKWAIT();
            OMPTR_TASK_END();
        }
    }

    OMPTR_PRINT("simple_v2.json");
    return 0;
}

void main_task(int taskID, int *omptr_bb_id_ptr) {
    OMPTR_NEW_CONTEXT();
    OMPTR_BEFORE_TASK();
    #pragma omp task firstprivate(omptr_bb_id)
    {
        OMPTR_TASK_START();
        printf("Task %d is being executed\n", taskID);
        OMPTR_TASK_END();
    }
    OMPTR_AFTER_TASK();

    printf("Additional work after task creation\n");
    OMPTR_END_CONTEXT();
}
