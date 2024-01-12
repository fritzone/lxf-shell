#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

int main() {

    // File descriptor for the output file, benevolently named output.txt
    int file_descriptor_for_stdout = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    int file_descriptor_for_stderr = open("errput.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (file_descriptor_for_stdout == -1 || file_descriptor_for_stderr == -1) 
    {
        perror("open failed");
        return 1;
    }

    // Use dup2 to redirect standard output (STDOUT) to the file handled by file_descriptor_for_stdout
    if (dup2(file_descriptor_for_stdout, STDOUT_FILENO) == -1) 
    {
        perror("dup2 for stdout");
        return 1;
    }

    // Use dup2 to redirect standard error (STDERR) to the file handled by file_descriptor_for_stderr
    if (dup2(file_descriptor_for_stderr, STDERR_FILENO) == -1) 
    {
        perror("dup2 for stderr");
        return 1;
    } 

    // Close the file descriptors as they are no longer needed after redirection
    close(file_descriptor_for_stdout);
    close(file_descriptor_for_stderr);

    // Print something to the standard output, and expect it to end up in their proper location
    fprintf(stdout, "Hello mon stdout duplicate!\n");
    fprintf(stderr, "Hello mon erroneous duplicate!\n");
}
