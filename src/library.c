#include "library.h"
#include <stdlib.h>

void TrimNewline(char* str) {
    if (!str) return;
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') str[len - 1] = '\0';
}

#ifdef _WIN32
#include <windows.h>

void* CpCreateSharedMemory(const char* name, size_t size) {
    HANDLE hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,    
        NULL,                    
        PAGE_READWRITE,          
        0,                     
        (DWORD)size,           
        name);                   

    if (hMapFile == NULL) {
        fprintf(stderr, "Error creating shared memory (%lu)\n", GetLastError());
        return NULL;
    }

    void* ptr = MapViewOfFile(
        hMapFile,               
        FILE_MAP_ALL_ACCESS,    
        0, 0, size);

    if (ptr == NULL) {
        fprintf(stderr, "Error mapping shared memory (%lu)\n", GetLastError());
        CloseHandle(hMapFile);
        return NULL;
    }

    return ptr;
}

void* CpOpenSharedMemory(const char* name, size_t size) {
    HANDLE hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,    
        FALSE,                
        name);                  

    if (hMapFile == NULL) {
        fprintf(stderr, "Error opening shared memory (%lu)\n", GetLastError());
        return NULL;
    }

    void* ptr = MapViewOfFile(
        hMapFile,               
        FILE_MAP_ALL_ACCESS,    
        0, 0, size);

    if (ptr == NULL) {
        fprintf(stderr, "Error mapping shared memory (%lu)\n", GetLastError());
        CloseHandle(hMapFile);
        return NULL;
    }

    return ptr;
}

void CpCloseSharedMemory(const char* name, void* ptr, size_t size) {
    if (ptr != NULL) {
        UnmapViewOfFile(ptr);
    }
}

int CpProcessCreate(process_t* proc, const char* path) {
    if (!proc || !path) return -1;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE childStdoutRead = NULL;
    HANDLE childStdoutWrite = NULL;
    HANDLE childStdinRead = NULL;
    HANDLE childStdinWrite = NULL;

    if (!CreatePipe(&childStdoutRead, &childStdoutWrite, &sa, 0)) return -1;
    if (!CreatePipe(&childStdinRead, &childStdinWrite, &sa, 0)) {
        CloseHandle(childStdoutRead);
        CloseHandle(childStdoutWrite);
        return -1;
    }

    SetHandleInformation(childStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(childStdinWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = childStdoutWrite;
    si.hStdOutput = childStdoutWrite;
    si.hStdInput = childStdinRead;
    si.dwFlags |= STARTF_USESTDHANDLES;

    char cmdline[1024];
    strncpy(cmdline, path, sizeof(cmdline)-1);
    cmdline[sizeof(cmdline)-1] = '\0';

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(childStdoutRead);
        CloseHandle(childStdoutWrite);
        CloseHandle(childStdinRead);
        CloseHandle(childStdinWrite);
        return -1;
    }

    CloseHandle(childStdoutWrite);
    CloseHandle(childStdinRead);

    proc->handle = pi.hProcess;
    proc->stdinWrite = childStdinWrite;
    proc->stdoutRead = childStdoutRead;

    CloseHandle(pi.hThread);
    return 0;
}

int CpProcessWrite(process_t* proc, const char* data, size_t size) {
    if (!proc || !data) return -1;
    DWORD written = 0;
    if (!WriteFile(proc->stdinWrite, data, (DWORD)size, &written, NULL)) {
        return -1;
    }
    return (int)written;
}

int CpProcessRead(process_t* proc, char* buffer, size_t size) {
    if (!proc || !buffer || size == 0) return -1;
    DWORD readBytes = 0;
    if (!ReadFile(proc->stdoutRead, buffer, (DWORD)(size - 1), &readBytes, NULL)) {
        return -1;
    }
    buffer[readBytes] = '\0';
    return (int)readBytes;
}

int CpProcessClose(process_t* proc) {
    if (!proc) return -1;
    int exitCode = -1;
    if (proc->stdinWrite) {
        CloseHandle(proc->stdinWrite);
        proc->stdinWrite = NULL;
    }
    if (proc->stdoutRead) {
        CloseHandle(proc->stdoutRead);
        proc->stdoutRead = NULL;
    }
    if (proc->handle) {
        WaitForSingleObject(proc->handle, INFINITE);
        DWORD code;
        if (GetExitCodeProcess(proc->handle, &code)) {
            exitCode = (int)code;
        }
        CloseHandle(proc->handle);
        proc->handle = NULL;
    }
    return exitCode;
}

#else

#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

void* CpCreateSharedMemory(const char* name, size_t size) { return NULL; }
void* CpOpenSharedMemory(const char* name, size_t size) { return NULL; }
void CpCloseSharedMemory(const char* name, void* ptr, size_t size) {}

int CpProcessCreate(process_t* proc, const char* path) {
    if (!proc || !path) return -1;
    int inpipe[2];
    int outpipe[2];

    if (pipe(inpipe) == -1) return -1;
    if (pipe(outpipe) == -1) {
        close(inpipe[0]); close(inpipe[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(inpipe[0]); close(inpipe[1]);
        close(outpipe[0]); close(outpipe[1]);
        return -1;
    }

    if (pid == 0) {
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);

        close(inpipe[0]); close(inpipe[1]);
        close(outpipe[0]); close(outpipe[1]);

        execl(path, path, (char*)NULL);
        _exit(127);
    } else {
        close(inpipe[0]);
        close(outpipe[1]);
        proc->pid = pid;
        proc->stdin_fd = inpipe[1];
        proc->stdout_fd = outpipe[0];
        return 0;
    }
}

int CpProcessWrite(process_t* proc, const char* data, size_t size) {
    if (!proc || !data) return -1;
    ssize_t n = write(proc->stdin_fd, data, size);
    if (n == -1) return -1;
    return (int)n;
}

int CpProcessRead(process_t* proc, char* buffer, size_t size) {
    if (!proc || !buffer || size == 0) return -1;
    ssize_t n = read(proc->stdout_fd, buffer, (ssize_t)(size - 1));
    if (n == -1) return -1;
    if (n == 0) {
        buffer[0] = '\0';
        return 0;
    }
    buffer[n] = '\0';
    return (int)n;
}

int CpProcessClose(process_t* proc) {
    if (!proc) return -1;
    int status = -1;
    if (proc->stdin_fd != -1) {
        close(proc->stdin_fd);
        proc->stdin_fd = -1;
    }
    if (proc->stdout_fd != -1) {
        close(proc->stdout_fd);
        proc->stdout_fd = -1;
    }
    if (proc->pid > 0) {
        waitpid(proc->pid, &status, 0);
        if (WIFEXITED(status)) return WEXITSTATUS(status);
    }
    return -1;
}

#endif

size_t CpStringLength(const char* str) {
    return str ? strlen(str) : 0;
}

int CpStringContains(const char* str, const char* substr) {
    if (!str || !substr) return 0;
    return strstr(str, substr) != NULL;
}