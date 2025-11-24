#include "library.h"
#include "shared_data.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

int main(void) {
    process_t child;
    char fileName[BUFFER_SIZE];
    char inputBuffer[BUFFER_SIZE];
    
    const char* shm_name = "Local\\DivisionLab3";
    SharedData* shared_data = (SharedData*)CpCreateSharedMemory(shm_name, sizeof(SharedData));
    
    if (shared_data == NULL) {
        fprintf(stderr, "Error: Failed to create shared memory\n");
        return EXIT_FAILURE;
    }
    
    memset(shared_data, 0, sizeof(SharedData));
    shared_data->data_ready = 0;
    shared_data->processing_complete = 1;
    shared_data->division_by_zero = 0;
    shared_data->terminate_process = 0;
    shared_data->numbers_count = 0;

    printf("Enter file name: ");
    if (!fgets(fileName, sizeof(fileName), stdin)) {
        fprintf(stderr, "Error: failed to read file name\n");
        CpCloseSharedMemory(shm_name, shared_data, sizeof(SharedData));
        return EXIT_FAILURE;
    }
    TrimNewline(fileName);
    if (strlen(fileName) == 0) {
        fprintf(stderr, "Error: empty file name\n");
        CpCloseSharedMemory(shm_name, shared_data, sizeof(SharedData));
        return EXIT_FAILURE;
    }

    strncpy(shared_data->filename, fileName, FILENAME_LENGTH - 1);
    shared_data->filename[FILENAME_LENGTH - 1] = '\0';

    char cmdLine[BUFFER_SIZE * 2];
    snprintf(cmdLine, sizeof(cmdLine), "child.exe \"%s\"", fileName);

    if (CpProcessCreate(&child, cmdLine) != 0) {
        fprintf(stderr, "Error: failed to create child process\n");
        CpCloseSharedMemory(shm_name, shared_data, sizeof(SharedData));
        return EXIT_FAILURE;
    }

    printf("Child process created. Using FILE MAPPING for IPC.\n");
    printf("Enter numbers separated by spaces (cumulative division):\n");
    printf("For exit write 'exit' or empty line\n");

    while (1) {
        printf("> ");
        fflush(stdout);
        
        if (!fgets(inputBuffer, sizeof(inputBuffer), stdin)) {
            break;
        }
        TrimNewline(inputBuffer);

        if (strcmp(inputBuffer, "exit") == 0 || strlen(inputBuffer) == 0) {
            shared_data->terminate_process = 1;
            shared_data->data_ready = 1;
            break;
        }

        int numbers[MAX_NUMBERS];
        int count = 0;
        char* token = strtok(inputBuffer, " \t");
        
        while (token != NULL && count < MAX_NUMBERS) {
            int valid = 1;
            for (int i = 0; token[i] != '\0'; i++) {
                if (!isdigit(token[i]) && !(i == 0 && token[i] == '-')) {
                    valid = 0;
                    break;
                }
            }
            
            if (valid) {
                numbers[count++] = atoi(token);
            }
            token = strtok(NULL, " \t");
        }

        if (count < 2) {
            printf("Error: need at least 2 numbers\n");
            continue;
        }

        shared_data->numbers_count = count;
        for (int i = 0; i < count; i++) {
            shared_data->numbers[i] = numbers[i];
        }
        
        shared_data->data_ready = 1;
        shared_data->processing_complete = 0;

        printf("Waiting for child to process...\n");
        while (!shared_data->processing_complete && !shared_data->division_by_zero) {
            Sleep(10);
        }

        if (shared_data->division_by_zero) {
            printf("EMERGENCY: Division by zero detected! Shutting down...\n");
            break;
        }
        
        printf("Processing complete. Results written to file.\n");
    }

    // Cleanup
    printf("Shutting down...\n");
    int exitCode = CpProcessClose(&child);
    CpCloseSharedMemory(shm_name, shared_data, sizeof(SharedData));
    
    printf("Parent process finished. Child exit code: %d\n", exitCode);
    return EXIT_SUCCESS;
}