#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "library.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define BUFFER_SIZE 2048

int main(int argc, char* argv[]) {
    char buffer[BUFFER_SIZE];
    FILE* file = NULL;

    if (argc != 2) {
        const char* err = "Error: expected filename as command line argument\n";
        printf("%s", err); 
        fflush(stdout);
        return EXIT_FAILURE;
    }

    const char* filename = argv[1];
    file = fopen(filename, "w");
    if (file == NULL) {
        char err[BUFFER_SIZE];
        snprintf(err, sizeof(err), "Error: cannot open file '%s' for writing: %s\n", 
                filename, strerror(errno));
        printf("%s", err); 
        fflush(stdout);
        return EXIT_FAILURE;
    }

    fprintf(file, "Child process started. Output file: %s\n", filename);
    fflush(file);
    
    printf("File opened successfully: %s\n", filename); 
    fflush(stdout);

    while (fgets(buffer, sizeof(buffer), stdin)) {
        TrimNewline(buffer);

        if (strlen(buffer) == 0) {
            break;
        }

        int values_count = 0;
        int values[256]; 
        char *tok = strtok(buffer, " \t");
        int parse_error = 0;
        
        while (tok != NULL && values_count < (int)(sizeof(values)/sizeof(values[0]))) {
            int valid = 1;
            for (int i = 0; tok[i] != '\0'; i++) {
                if (!isdigit(tok[i]) && !(i == 0 && tok[i] == '-')) {
                    valid = 0;
                    break;
                }
            }
            
            if (valid) {
                values[values_count++] = atoi(tok);
            } else {
                parse_error = 1;
                break;
            }
            tok = strtok(NULL, " \t");
        }

        if (values_count == 0) {
            fprintf(file, "Error: no valid numbers found in input\n");
            fflush(file);
            continue;
        }

        if (parse_error) {
            fprintf(file, "Error: failed to parse integers correctly\n");
            fflush(file);
            continue;
        }

        if (values_count < 2) {
            fprintf(file, "Error: minimum 2 numbers required, got %d\n", values_count);
            fflush(file);
            continue;
        }

        fprintf(file, "Input: ");
        for (int i = 0; i < values_count; i++) {
            fprintf(file, "%d ", values[i]);
        }
        fprintf(file, "\n");

        int divzero = 0;
        for (int i = 1; i < values_count; ++i) {
            if (values[i] == 0) {
                fprintf(file, "ERROR: Division by zero! %d / %d\n", values[0], values[i]);
                divzero = 1;
                break;
            }
        }

        if (divzero) {
            fprintf(file, "EMERGENCY SHUTDOWN: Division by zero detected\n");
            fflush(file);
            fclose(file);
            #ifdef _WIN32
                ExitProcess(2);  
            #else
                exit(2);
            #endif
        }

        float result = (float)values[0];
        fprintf(file, "Division results:\n");
        for (int i = 1; i < values_count; i++) {
            float temp = result / (float)values[i];
            fprintf(file, "  %.2f / %.2f = %.2f\n", result, (float)values[i], temp);
            result = temp;
        }

        fprintf(file, "---\n");
        fflush(file);
    }

    fprintf(file, "Child process finished normally\n");
    if (file) fclose(file);
    printf("Child process finished\n");
    fflush(stdout);
    return EXIT_SUCCESS;
}