#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <libexplain/execvp.h>

int execute(const std::string& program) 
{
    pid_t child_pid;

    // Create a new process using fork()
    child_pid = fork();

    if (child_pid == -1) 
    {
        // Forking failed
        std::perror("fork");
        return 1;
    }

    if (child_pid == 0) 
    {
        // This code runs in the child process

        // Execute a new program in the child process using exec()
        const char* const arguments[] = {program.c_str(), nullptr};

        if (execvp(program.c_str(), (char* const*)arguments) == -1) 
        {
            std::cerr << explain_execvp(program.c_str(), (char* const*)arguments) << std::endl;
            return 1;
        }
    } 
    else 
    {
        // This code runs in the parent process

        // Wait for the child process to complete
        int status;
        waitpid(child_pid, &status, 0);
    }

    return 0;
}

int main()
{
	while(true)
	{
		std::cout << "lxfsh$ ";
		std::string command;
		std::getline(std::cin, command);
		if(command == "exit")
		{
			exit(0);
		}
		execute(command);
	}
}