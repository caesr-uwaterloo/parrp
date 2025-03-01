#include <omp.h>
#include <stdio.h>

void main_task(int taskID);

int main() {

    int max_threads = 8;
    omp_set_num_threads(max_threads);

    #pragma omp parallel
    {
        #pragma omp single
        {
            // The main task creates and executes three subtasks
            #pragma omp task
            {
                main_task(1);
            }

            // Additional work before creating the next task
            printf("Additional work between task creations\n");

            #pragma omp task
            {
                main_task(2);
            }

            // Additional work before creating the next task
            printf("Additional work between task creations\n");

            #pragma omp task
            {
                main_task(3);
            }

            // Explicitly wait for all tasks to complete
            #pragma omp taskwait
        }
    }

    return 0;
}

void main_task(int taskID) {
    printf("Task %d is being executed\n", taskID);
}
