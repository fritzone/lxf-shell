#include <libexplain/execvp.h>
#include <libexplain/fork.h>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <memory>

/**
 * Will split the given string 
 **/
std::vector<std::string> splitStringByWhitespace(const std::string& input) 
{
    std::vector<std::string> result;
    std::istringstream iss(input);
    std::string token;

    while (iss >> token) 
    {
        result.push_back(token);
    }

    return result;
}

/**
 * Will execute the given command
 **/
int execute(const std::string& program) 
{
    if(program.empty())
    {
        return 1;
    }

    pid_t child_pid;
    
    // Create a new process using fork()
    child_pid = fork();

    if (child_pid == -1) 
    {
        // Forking failed
        std::cerr << explain_fork() << std::endl;
        return 1;
    }

    if (child_pid == 0) 
    {
        // This code runs in the child process

        // First: let's split the command into separate tokens
        auto split = splitStringByWhitespace(program);

        // Then create the arguments array
        const char** arguments = new const char*[split.size() + 1];
        std::unique_ptr<const char*> r;
        r.reset(arguments);

        size_t i = 0;
        for(; i<split.size(); i++)
        {
            arguments[i] = split[i].c_str();
        }
        arguments[i] = nullptr;

        if (execvp(arguments[0], (char* const*)(arguments)) == -1) 
        {
            std::cerr << explain_execvp(program.c_str(), (char* const*)(arguments)) << std::endl;
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