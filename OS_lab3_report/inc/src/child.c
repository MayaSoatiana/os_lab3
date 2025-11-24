#include "library.h"
#include "shared_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    FILE* output_file = NULL;

    const char* shm_name = "Local\\DivisionLab3";
    SharedData* shared_data = (SharedData*)CpOpenSharedMemory(shm_name, sizeof(SharedData));
    
    if (shared_data == NULL) {
        fprintf(stderr, "Error: Failed to open shared memory\n");
        return EXIT_FAILURE;
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        CpCloseSharedMemory(shm_name, shared_data, sizeof(SharedData));
        return EXIT_FAILURE;
    }

    const char* filename = argv[1];

    output_file = fopen(filename, "w");
    if (output_file == NULL) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        CpCloseSharedMemory(shm_name, shared_data, sizeof(SharedData));
        return EXIT_FAILURE;
    }

    fprintf(output_file, "Child process started. Output file: %s\n", filename);
    fprintf(output_file, "Using FILE MAPPING for IPC with parent\n");
    fflush(output_file);

    printf("Child process started. Monitoring shared memory...\n");

    while (!shared_data->terminate_process) {
        if (shared_data->data_ready) {
            if (shared_data->numbers_count < 2) {
                fprintf(output_file, "Error: need at least 2 numbers, got %d\n", shared_data->numbers_count);
            } else {
                fprintf(output_file, "Input: ");
                for (int i = 0; i < shared_data->numbers_count; i++) {
                    fprintf(output_file, "%d ", shared_data->numbers[i]);
                }
                fprintf(output_file, "\n");

                int division_by_zero = 0;
                for (int i = 1; i < shared_data->numbers_count; i++) {
                    if (shared_data->numbers[i] == 0) {
                        fprintf(output_file, "ERROR: Division by zero! %d / %d\n", 
                                shared_data->numbers[0], shared_data->numbers[i]);
                        division_by_zero = 1;
                        shared_data->division_by_zero = 1;
                        break;
                    }
                }

                if (!division_by_zero) {
                    float result = (float)shared_data->numbers[0];
                    fprintf(output_file, "Division results:\n");
                    for (int i = 1; i < shared_data->numbers_count; i++) {
                        float temp = result / (float)shared_data->numbers[i];
                        fprintf(output_file, "  %.2f / %.2f = %.2f\n", 
                                result, (float)shared_data->numbers[i], temp);
                        result = temp;
                    }
                    fprintf(output_file, "Final result: %.2f\n", result);
                    fprintf(output_file, "---\n");
                }
            }
            
            fflush(output_file);
            shared_data->processing_complete = 1;
            shared_data->data_ready = 0;

            if (shared_data->division_by_zero) {
                fprintf(output_file, "EMERGENCY SHUTDOWN: Division by zero detected\n");
                fflush(output_file);
                fclose(output_file);
                CpCloseSharedMemory(shm_name, shared_data, sizeof(SharedData));
                
                #ifdef _WIN32
                    ExitProcess(2);
                #else
                    exit(2);
                #endif
            }
        }
        
        #ifdef _WIN32
            Sleep(10);
        #else
            usleep(10000);
        #endif
    }

    fprintf(output_file, "Child process finished normally\n");
    fclose(output_file);
    CpCloseSharedMemory(shm_name, shared_data, sizeof(SharedData));
    
    printf("Child process finished\n");
    return EXIT_SUCCESS;
}