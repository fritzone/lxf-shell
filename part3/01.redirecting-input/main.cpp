#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    pid_t child_pid;
    int pipe_fd[2];
    pipe(pipe_fd);
    child_pid = fork();
    if (child_pid == 0) {
        close(pipe_fd[1]);

        // Redirect stdin to read from the pipe
        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[0]);

        char program_name[] = "wc"; // Replace with the name of the program you want to execute

        // Execute the program with its arguments
        char *args[] = {program_name, NULL};
        execvp(program_name, args);

        // If execvp fails, it will reach this point
        perror("execvp failed");
        exit(1);
    } else {
        // Parent process
        close(pipe_fd[0]); // Close the read end of the pipe

        // Open the file to be used as input
        FILE *input_file = fopen("input.txt", "r"); // Replace "input.txt" with the name of your input file
        if (input_file == NULL) {
            perror("Error opening input file");
            exit(1);
        }

        char buffer[4096];
        size_t bytesRead;

        // Read from the input file and write to the pipe connected to the child's stdin
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), input_file)) > 0) {
            write(pipe_fd[1], buffer, bytesRead);
        }

        close(pipe_fd[1]); // Close the write end of the pipe

        // Wait for the child process to finish
        wait(NULL);
    }

    return 0;
}
