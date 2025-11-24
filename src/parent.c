#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "library.h"

#define BUFFER_SIZE 2048

int main(void) {
    process_t child;
    char line[BUFFER_SIZE];
    char fileName[BUFFER_SIZE];
    int result;

    memset(&child, 0, sizeof(child));
    
    printf("Enter file name: ");
    if (!fgets(fileName, sizeof(fileName), stdin)) {
        fprintf(stderr, "Error: failed to read file name from parent stdin\n");
        return EXIT_FAILURE;
    }
    TrimNewline(fileName);
    if (strlen(fileName) == 0) {
        fprintf(stderr, "Error: empty file name\n");
        return EXIT_FAILURE;
    }

    const char *childPath = CpGetChildProcessName("child");
    char cmdLine[BUFFER_SIZE * 2];
    snprintf(cmdLine, sizeof(cmdLine), "%s \"%s\"", childPath, fileName);

    if (CpProcessCreate(&child, cmdLine) != 0) {
        fprintf(stderr, "Error: failed to create child process\n");
        return EXIT_FAILURE;
    }

    printf("Child process created. Enter commands (numbers separated by spaces):\n");
    printf("Enter 'exit' to quit\n");

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            CpProcessWrite(&child, "\n", 1);
            break;
        }
        TrimNewline(line);

        if (strcmp(line, "exit") == 0) {
            CpProcessWrite(&child, "\n", 1);
            break;
        }

        if (strlen(line) == 0) {
            CpProcessWrite(&child, "\n", 1);
            break;
        }

        if (CpProcessWrite(&child, line, strlen(line)) < 0 ||
            CpProcessWrite(&child, "\n", 1) < 0) {
            fprintf(stderr, "Error: failed to send numbers to child process\n");
            break;
        }

        #ifdef _WIN32
            DWORD exitCode;
            if (GetExitCodeProcess(child.handle, &exitCode) && exitCode != STILL_ACTIVE) {
                printf("Child process terminated with code %lu\n", exitCode);
                break;
            }
            Sleep(50); 
        #else
            usleep(50000);
        #endif
    }

    int exitCode = CpProcessClose(&child);
    printf("Parent process finished. Child exit code: %d\n", exitCode);
    return EXIT_SUCCESS;
}