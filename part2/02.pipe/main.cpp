#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main() {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return 1;
    }
    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        return 1;
    }
    if (child_pid == 0) {
        close(pipe_fd[1]);
        char message[100];
        ssize_t bytes_read = read(pipe_fd[0], message, sizeof(message));
        if (bytes_read > 0) {
            printf("Pipeling received: %.*s\n", (int)bytes_read, message);
        }
        close(pipe_fd[0]);
    } else {
        close(pipe_fd[0]);
        const char* message = "Hello pipeling!";
        write(pipe_fd[1], message, strlen(message));
        close(pipe_fd[1]);
    }

    return 0;
}