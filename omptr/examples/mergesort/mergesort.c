#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define ARGUMENT_SIZE 1024

void scrambleArray(int arr[], int size) {
    // Scramble the array content randomly
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        // Swap arr[i] and arr[j]
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

void merge(int arr[], int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    // Create temporary arrays
    int L[n1], R[n2];

    // Copy data to temporary arrays L[] and R[]
    for (int i = 0; i < n1; i++)
        L[i] = arr[left + i];
    for (int j = 0; j < n2; j++)
        R[j] = arr[mid + 1 + j];

    // Merge the temporary arrays back into arr[left..right]
    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }

    // Copy the remaining elements of L[], if there are any
    while (i < n1) {
        arr[k] = L[i];
        i++;
        k++;
    }

    // Copy the remaining elements of R[], if there are any
    while (j < n2) {
        arr[k] = R[j];
        j++;
        k++;
    }
}

void mergeSort(int arr[], int left, int right) {
    if (left < right) {
        // Find the middle point
        int mid = left + (right - left) / 2;

        // Recursively sort the first and second halves
        #pragma omp task firstprivate(omptr_bb_id)
        mergeSort(arr, left, mid);

        #pragma omp task firstprivate(omptr_bb_id)
        mergeSort(arr, mid + 1, right);

        // Merge the sorted halves
        #pragma omp taskwait
        merge(arr, left, mid, right);
    }
}

int main() {

    int max_threads = 8;
    omp_set_num_threads(max_threads);

    // Allocate memory for the array dynamically
    int *arr = (int *)malloc(ARGUMENT_SIZE * sizeof(int));
    for (int i = 0; i < ARGUMENT_SIZE; i++) {
        arr[i] = i + 1;
    }
    scrambleArray(arr, ARGUMENT_SIZE);


    // Perform parallel merge sort
    #pragma omp parallel
    {
        #pragma omp single
        mergeSort(arr, 0, ARGUMENT_SIZE - 1);
    }

    // Check correctness
    for (int i = 1; i < ARGUMENT_SIZE; i++) {
        if (arr[i - 1] > arr[i]) {
            printf("Sorting failed: Incorrect order at index %d\n", i);
            free(arr);
            return 1;
        }
    }

    printf("Sorting is correct!\n");
    free(arr);

    return 0;
}
