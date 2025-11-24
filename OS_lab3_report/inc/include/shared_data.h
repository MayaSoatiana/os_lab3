#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#define MAX_NUMBERS 100
#define FILENAME_LENGTH 256

typedef struct {
    char filename[FILENAME_LENGTH];
    int numbers[MAX_NUMBERS];
    int numbers_count;
    int data_ready;
    int processing_complete;
    int division_by_zero;
    int terminate_process;
} SharedData;

#endif