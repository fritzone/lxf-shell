#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    int file_descriptor; // File descriptor for the output file
    file_descriptor = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (file_descriptor == -1) {
        return 1;
    }
    if (dup2(file_descriptor, STDOUT_FILENO) == -1) {
        return 1;
    }
    close(file_descriptor);
    const char * const command[] = {"ls", "-l", NULL};
    execvp("ls", (char* const*)command);
    return 1;
}