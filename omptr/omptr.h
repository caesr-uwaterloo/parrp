#ifndef __OMPTR_H__
#define __OMPTR_H__

#define MAX_NUM_BB 10000
#define MAX_NUM_TASK 10000
#define MAX_TASK_CHILDREN 10000

#include <stdio.h>
#include <stdlib.h>

// structs
struct BasicBlock {
     int taskID;                        // ID of task this basic blocks belongs to
     int nodeID;                        // ID of corresponding task node
     int taskCreated;                   // ID of task this basic block creates
     int numTasksWaitingFor;            // Number of tasks this basic block waits for
     int waitFor[MAX_TASK_CHILDREN];    // ID of tasks this basic block waits for
};

typedef struct BasicBlock BasicBlock;

struct OmptrTask {
     int numChildren;
     int children[MAX_TASK_CHILDREN];
};

typedef struct OmptrTask OmptrTask;

// global vars
BasicBlock *BBs;
OmptrTask *Tasks;
int Task_Counter;
int BB_Counter;

// functions
int omptr_init() {
     Task_Counter = 0;
     BB_Counter = 0;

     BBs = NULL;
     Tasks = NULL;
     BBs = (BasicBlock*)malloc(MAX_NUM_BB * sizeof(BasicBlock));
     if (BBs == NULL) {
          printf("OMPTR ERROR: not enough memory!\n");
     }
     Tasks = (OmptrTask*)malloc(MAX_NUM_TASK * sizeof(OmptrTask));
     if (Tasks == NULL) {
          printf("OMPTR ERROR: not enough memory!\n");
     }

     for (int i = 0; i < MAX_NUM_BB; ++i) {
          BBs[i].taskID = 0;
          BBs[i].nodeID = 0;
          BBs[i].taskCreated = -1;
          BBs[i].numTasksWaitingFor = 0;
     }

     for (int i = 0; i < MAX_NUM_TASK; ++i) {
          Tasks[i].numChildren = 0;
     }

     return 0;
}

/*
 * This function populates the next basic block of the parent task and the starting basic block of the child task.
 * Unintuitively, this function sets the parent task bb_id to be the child task bb_id.
 * The reason is that bb_id of child task is passed by firstprivate clause in openmp task directive 
 * (For details, check OMPTR macro and example usage).
 * The new bb_id of the parent task is returned as a copy instead.
 */
int omptr_task(int *bb_id_ptr) {
     BasicBlock *bb = &BBs[*bb_id_ptr];
     OmptrTask *task = &Tasks[bb->taskID];

     int new_task_id;
     int new_task_bb_id;
     int new_bb_id;

     // create child task
     #pragma omp atomic capture
     new_task_id = ++Task_Counter;

     if (new_task_id >= MAX_NUM_TASK) {
         printf("OMPTR ERROR: maximum number of tasks exceeds!\n");
         exit(-1); 
     }
     OmptrTask *new_task = &Tasks[new_task_id];

     // create bb for child task
     #pragma omp atomic capture
     new_task_bb_id = ++BB_Counter;

     BasicBlock *new_task_bb = &BBs[new_task_bb_id];
     new_task_bb->taskID = new_task_id;
     
     // create new bb for current task
     #pragma omp atomic capture
     new_bb_id = ++BB_Counter;

     if (new_bb_id >= MAX_NUM_BB) {
          printf("OMPTR ERROR: maximum number of basic blocks exceeds!\n");
          exit(-1); 
     }
     BasicBlock *new_bb = &BBs[new_bb_id];
     new_bb->taskID = bb->taskID;
     new_bb->nodeID = bb->nodeID + 1;

     // record task creation for current bb
     bb->taskCreated = new_task_id;

     // record children for current task
     task->children[task->numChildren++] = new_task_id;
     if (task->numChildren > MAX_TASK_CHILDREN) {
          printf("OMPTR ERROR: maximum number of task children exceeds!\n");
          exit(-1); 
     }

     // return value
     *bb_id_ptr = new_task_bb_id;
     return new_bb_id;
}

void omptr_task_wait(int *bb_id_ptr) {
     BasicBlock *bb = &BBs[*bb_id_ptr];
     OmptrTask *task = &Tasks[bb->taskID];

     int new_bb_id;
     // create new bb for current task
     #pragma omp atomic capture
     new_bb_id = ++BB_Counter;

     if (new_bb_id >= MAX_NUM_BB) {
          printf("OMPTR ERROR: maximum number of basic blocks exceeds!\n");
          exit(-1); 
     }
     BasicBlock *new_bb = &BBs[new_bb_id];
     new_bb->taskID = bb->taskID;
     new_bb->nodeID = bb->nodeID + 1;

     // record tasks waiting for
     for (int i = 0; i < task->numChildren; ++i) {
          new_bb->waitFor[i] = task->children[i];
     }
     new_bb->numTasksWaitingFor = task->numChildren;
     task->numChildren = 0;  // reset task children

     *bb_id_ptr = new_bb_id;
}

void printBasicBlockJSON(FILE *file, BasicBlock bb, int bb_id) {
    fprintf(file, "{\n");
    fprintf(file, "  \"ID\": %d,\n", bb_id);
    fprintf(file, "  \"taskID\": %d,\n", bb.taskID);
    fprintf(file, "  \"nodeID\": %d,\n", bb.nodeID);
    fprintf(file, "  \"taskCreated\": %d,\n", bb.taskCreated);
    fprintf(file, "  \"numTasksWaitingFor\": %d,\n", bb.numTasksWaitingFor);
    fprintf(file, "  \"waitFor\": [");
    for (int i = 0; i < bb.numTasksWaitingFor; i++) {
        fprintf(file, "%d", bb.waitFor[i]);
        if (i < bb.numTasksWaitingFor - 1) {
            fprintf(file, ", ");
        }
    }
    fprintf(file, "]\n");
    fprintf(file, "}\n");
}

void omptr_print(const char *filename) {
     FILE *outputFile;

     // Open the file for writing
     if ((outputFile = fopen(filename, "w")) == NULL) {
          fprintf(stderr, "OMPTR ERROR: Error opening file for writing.\n");
          exit(-1);
     }

     fprintf(outputFile, "[\n");
     for (int i = 0; i <= BB_Counter; ++i) {
          printBasicBlockJSON(outputFile, BBs[i], i);
          if (i < BB_Counter) {
               fprintf(outputFile, ",\n");
          }
     }
     fprintf(outputFile, "]\n");
     fclose(outputFile);
     free(BBs);
     free(Tasks);
}

// OMPTR macros
#define OMPTR_INIT() \
     int omptr_bb_id = omptr_init(); \
     int omptr_new_bb_id;

#define OMPTR_TASK_START() \
     asm volatile("" ::: "memory"); \
     printf("[OMPTR] BB %d starts.\n", omptr_bb_id); \
     asm volatile("" ::: "memory");

#define OMPTR_TASK_END() \
     asm volatile("" ::: "memory"); \
     printf("[OMPTR] BB %d ends.\n", omptr_bb_id); \
     asm volatile("" ::: "memory");

#define OMPTR_NEW_CONTEXT() \
     asm volatile("" ::: "memory"); \
     int omptr_bb_id = *omptr_bb_id_ptr; \
     int omptr_new_bb_id; \
     asm volatile("" ::: "memory");

#define OMPTR_END_CONTEXT() \
     asm volatile("" ::: "memory"); \
     *omptr_bb_id_ptr = omptr_bb_id; \
     asm volatile("" ::: "memory");

#define OMPTR_BEFORE_TASK() \
     asm volatile("" ::: "memory"); \
     printf("[OMPTR] BB %d ends.\n", omptr_bb_id); \
     omptr_new_bb_id = omptr_task(&omptr_bb_id); \
     asm volatile("" ::: "memory");

#define OMPTR_AFTER_TASK() \
     asm volatile("" ::: "memory"); \
     omptr_bb_id = omptr_new_bb_id; \
     printf("[OMPTR] BB %d starts.\n", omptr_bb_id); \
     asm volatile("" ::: "memory");

#define OMPTR_BEFORE_TASKWAIT() \
     asm volatile("" ::: "memory"); \
     printf("[OMPTR] BB %d ends.\n", omptr_bb_id); \
     omptr_task_wait(&omptr_bb_id); \
     asm volatile("" ::: "memory");

#define OMPTR_AFTER_TASKWAIT() \
     asm volatile("" ::: "memory"); \
     printf("[OMPTR] BB %d starts.\n", omptr_bb_id); \
     asm volatile("" ::: "memory");

#define OMPTR_PRINT(fn) \
     omptr_print(fn);

// Instumentation Rules:
// 1. Main function (Note: make sure OMPTR_TASK_START() AND OMPTR_TASK_END() are placed in the same scope)
//   OMPTR_INIT();
//   #pragma omp single
//   {
//        OMPTR_TASK_START();
//        ... // main task
//        OMPTR_TASK_END();
//   }
//   OMPTR_PRINT("omptr.json");
//
// 2. Rule for #pragma omp task
//   OMPTR_BEFORE_TASK();
//   #pragma omp task... firstprivate(omptr_bb_id)
//   {
//         OMPTR_TASK_START();
//         ... // task body
//         OMPTR_TASK_END();
//   }
//   OMPTR_AFTER_TASK();
// 
// 3. Rule for #pragma omp taskwait
//   OMPTR_BEFORE_TASKWAIT();
//   #pragma omp taskwait
//   OMPTR_AFTER_TASKWAIT();
//   
//
// 4. Rule for different contexts (OpenMP task pragma is placed in a different function named "new_context()")
//
//   Update the function to:  
//   new_context(..., int *omptr_bb_id_ptr) {
//        OMPTR_NEW_CONTEXT();
//        ...
//        OMPTR_BEFORE_TASK();
//        #pragma omp task... firstprivate(omptr_bb_id)
//        {
//            OMPTR_TASK_START();
//            ... // task body
//            OMPTR_TASK_END();
//        }
//        OMPTR_AFTER_TASK();  
//        ...
//        OMPTR_END_CONTEXT();     
//   }
//
//   Update the calling convention to:
//   new_context(..., &omptr_bb_id);

#endif
