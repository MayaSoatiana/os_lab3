#ifndef LIBRARY_H
#define LIBRARY_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

void TrimNewline(char* str);

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

#ifdef _WIN32
typedef struct {
    HANDLE handle;
    HANDLE stdinWrite;
    HANDLE stdoutRead;
} process_t;
#else
typedef struct {
    pid_t pid;
    int stdin_fd;
    int stdout_fd;
} process_t;
#endif

int  CpProcessCreate(process_t* proc, const char* path);
int  CpProcessWrite(process_t* proc, const char* data, size_t size);
int  CpProcessRead(process_t* proc, char* buffer, size_t size);
int  CpProcessClose(process_t* proc);

size_t CpStringLength(const char* str);
int    CpStringContains(const char* str, const char* substr);

static inline const char* CpGetChildProcessName(const char* baseName) {
    (void)baseName;
#ifdef _WIN32
    return "child.exe";
#else
    return "./child";
#endif
}

#define CpWriteStdout(data, size) ((int)fwrite((data), 1, (size), stdout))
#define CpWriteStderr(data, size) ((int)fwrite((data), 1, (size), stderr))

#endif
