#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>

#include <libexplain/execvp.h>
#include <libexplain/pipe.h>
#include <libexplain/open.h>
#include <libexplain/fopen.h>
#include <libexplain/fork.h>
#include <libexplain/dup2.h>

#include <sys/wait.h>
#include <fcntl.h>

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <sstream>


/**
 * @brief splitStringByWhitespace will split the given string by whitespace
 * @param input the string to split
 * @return a vector of strings
 */
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
 * @brief extractWordsAfterSequence This will extract the words from an input string that come after a specific sequence
 *
 * The function will modify the input variable, by removing the sequence and the extracted word from it.
 *
 * @param input
 * @param sequence
 * 
 * @return the words that were extracted after a specific sequence
 */
std::vector<std::string> extractWordsAfterSequence(std::string& input, const std::string& sequence)
{
    std::vector<std::string> result;
    size_t pos = 0, start = 0;
    while ((pos = input.find(sequence, start)) != std::string::npos) 
    {
        size_t end = pos + sequence.length();
        while (end < input.length() && std::iswspace( input[end] )) end++;
        size_t wordStart = end;
        while (wordStart < input.length() && !std::iswspace(input[wordStart]) && input.substr(wordStart, sequence.length()) != sequence)
        {
            wordStart++;
        }
        std::string word = input.substr(end, wordStart - end);
        result.push_back(word);
        input.erase(pos, wordStart - pos);
    }
    return result;
}

/**
 * @brief execute executes the given program.
 *
 * The program comes in the form of <appname> [parameters]. The function separates these into valid data.
 * There are a number of file descriptors that the stdout and stderr output can be redirected to, and
 * a file name from where the stdin is read from.
 *
 * @param program - the program and the parameters to execeute
 * @param stdoutFds - the file descriptors for the stdout
 * @param numStdoutFds - the number of file descriptors for stdout
 * @param stderrFds - the file descriptors for the stderr
 * @param numStderrFds - the number of file descriptors for stderr
 * @param stdinInputFile - the location of where to read the input data for isntead of stdin
 * @param stderrGoesToStdout - whether the stderr is redirected to stdout
 * @param stdoutGoesToStderr - whether the stdout is redirected to stderr
 *
 * @return 1 if there was any kind of error, 0 otherwise
 */
int execute(const std::string& program,
            int* stdoutFds, int numStdoutFds,
            int* stderrFds, int numStderrFds,
            const std::string& stdinInputFile,
            bool stderrGoesToStdout, bool stdoutGoesToStderr)
{
    const bool needsStdoutRedirect = numStdoutFds > 0;        // do we need to redirect stdout?
    const bool needsStderrRedirect = numStderrFds > 0;        // do we need to redirect stderr?
    const bool needsStdinRedirect = ! stdinInputFile.empty(); // If there is a stdin file, we open it, otherwise no stdin redirect
    int pipeStdoutFd[2] = {-1, -1};                     // Pipe for stdout
    int pipeStderrFd[2] = {-1, -1};                     // Pipe for stderr
    int pipeStdinFd[2] =  {-1, -1};                     // Pipe for stdin

    if(needsStdoutRedirect)
    {
        // Create a pipe to capture the command's output.
        if (pipe(pipeStdoutFd) == -1)
        {
            std::cerr << explain_pipe(pipeStdoutFd) << std::endl;
            return 1;
        }
    }

    if(needsStderrRedirect)
    {
        // Create a pipe to capture the command's output.
        if (pipe(pipeStderrFd) == -1)
        {
            std::cerr << explain_pipe(pipeStderrFd) << std::endl;
            return 1;
        }
    }

    if(needsStdinRedirect)
    {
        // Create a pipe for the stdin redirection
        if (pipe(pipeStdinFd) == -1)
        {
            std::cerr << explain_pipe(pipeStdinFd) << std::endl;
            return 1;
        }
    }

    // fork the process
    pid_t pid = fork();

    if (pid == -1)
    {
        std::cerr << explain_fork() << std::endl;
        return 1;
    }
    else
    if (pid == 0) { // Child process

        // Set up the STDOUT redirect
        if(needsStdoutRedirect)
        {
            close(pipeStdoutFd[0]);

            // Redirect stdout to the pipe.
            if(dup2(pipeStdoutFd[1], STDOUT_FILENO) == -1)
            {
                std::cerr << explain_dup2(pipeStdoutFd[1], STDOUT_FILENO ) << std::endl;
                return 1;
            }

            // Close the original file descriptors.
            close(pipeStdoutFd[1]);
            for (int i = 0; i < numStdoutFds; i++)
            {
                if(stdoutFds[i] != -1)
                {
                    close(stdoutFds[i]);
                }
            }
        }

        // Set up the STDERR redirect
        if(needsStderrRedirect)
        {
            close(pipeStderrFd[0]);

            // Redirect stderr to the pipe.
            if(dup2(pipeStderrFd[1], STDERR_FILENO) == -1)
            {
                std::cerr << explain_dup2(pipeStdoutFd[1], STDOUT_FILENO ) << std::endl;
                return 1;
            }

            // Close the original file descriptors.
            close(pipeStderrFd[1]);
            for (int i = 0; i < numStderrFds; i++)
            {
                if(stderrFds[i] != -1)
                {
                    close(stderrFds[i]);
                }
            }
        }

        if(needsStdinRedirect)
        {
            // Close the write end of the input pipe
            close(pipeStdinFd[1]);

            // Redirect stdin to read from the input pipe
            dup2(pipeStdinFd[0], STDIN_FILENO);
            close(pipeStdinFd[0]);
        }

        // Here we start the execution of the command

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
            close(pipeStdoutFd[1]);
            close(pipeStderrFd[1]);
            close(pipeStdoutFd[0]);
            close(pipeStderrFd[0]);

            for (int i = 0; i < numStdoutFds; i++)
            {
                if(stdoutFds[i] != -1)
                {
                    close(stdoutFds[i]);
                }
            }

            for (int i = 0; i < numStderrFds; i++)
            {
                if(stderrFds[i] != -1)
                {
                    close(stderrFds[i]);
                }
            }
            std::cerr << explain_execvp(program.c_str(), (char* const*)(arguments)) << std::endl;
            return 1;
        }
    }
    else // Parent process
    {

        if(needsStdinRedirect)
        {
            close(pipeStdinFd[0]); // Close the read end of the input pipe

            // Open the input file to be used
            FILE *input_file = fopen(stdinInputFile.c_str(), "r");
            if (input_file == NULL)
            {
                std::cerr << explain_fopen(stdinInputFile.c_str(), "r") << std::endl;
                return 1;
            }

            char buffer[4096] = {0};
            size_t bytesRead;

            // Read from the input file and write to the input pipe connected to the child's stdin
            while ((bytesRead = fread(buffer, 1, sizeof(buffer), input_file)) > 0)
            {
                write(pipeStdinFd[1], buffer, bytesRead);
            }

            fclose(input_file);

            close(pipeStdinFd[1]);
        }

        if(needsStdoutRedirect)
        {
            // Close the write end of the pipe since we're not using it.
            close(pipeStdoutFd[1]);

            // Create a buffer to capture the output from the pipe.
            char buffer[4096] = {0};
            ssize_t bytes_read;

            // Read the output from the pipe and write it to the output files.
            while ((bytes_read = read(pipeStdoutFd[0], buffer, sizeof(buffer))) > 0)
            {

                for (int i = 0; i < numStdoutFds; i++)
                {
                    if(stdoutFds[i] != -1)
                    {
                        write(stdoutFds[i], buffer, bytes_read);
                    }
                }
                if(stdoutGoesToStderr)
                {
                    for (int i = 0; i < numStderrFds; i++)
                    {
                        if(stderrFds[i] != -1)
                        {
                            write(stderrFds[i], buffer, bytes_read);
                        }
                    }
                }
            }

            // Close the pipe and wait for the child process to finish.
            close(pipeStdoutFd[0]);
        }

        if(needsStderrRedirect)
        {
            // Close the write end of the pipe since we're not using it.
            close(pipeStderrFd[1]);

            // Create a buffer to capture the errput from the pipe.
            char buffer[4096] = {0};
            ssize_t bytes_read;

            // Read the errput from the pipe and write it to the errput files.
            while ((bytes_read = read(pipeStderrFd[0], buffer, sizeof(buffer))) > 0)
            {
                for (int i = 0; i < numStderrFds; i++)
                {
                    if(stderrFds[i] != -1)
                    {
                        write(stderrFds[i], buffer, bytes_read);
                    }
                }
                if(stderrGoesToStdout)
                {
                    for (int i = 0; i < numStdoutFds; i++)
                    {
                        if(stdoutFds[i] != -1)
                        {
                            write(stdoutFds[i], buffer, bytes_read);
                        }
                    }
                }
            }

            // Close the pipe and wait for the child process to finish.
            close(pipeStderrFd[0]);
        }

        for (int i = 0; i < numStdoutFds; i++)
        {
            if(stdoutFds[i] != -1)
            {
                close(stdoutFds[i]);
            }
        }

        for (int i = 0; i < numStderrFds; i++)
        {
            if(stderrFds[i] != -1)
            {
                close(stderrFds[i]);
            }
        }

        // and wait for the PID
        waitpid(pid, NULL, 0);
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

        // gather all the files we redirect to
        std::vector<std::string> stderrAppendRedirects = extractWordsAfterSequence(command, "2>>");
        std::vector<std::string> stderrOverwriteRedirects = extractWordsAfterSequence(command, "2>");
        std::vector<std::string> stdoutAppendRedirects = extractWordsAfterSequence(command, ">>");
        std::vector<std::string> stdoutOverwriteRedirects = extractWordsAfterSequence(command, ">");

        // this will be the file from which we read the input redirect if any
        std::vector<std::string> stdinInputFiles = extractWordsAfterSequence(command, "<");
        std::string stdinInputFile = stdinInputFiles.empty() ? "" : stdinInputFiles[0];

        // Create an array of output files for stderr
        const int stderrNumOutputs = stderrAppendRedirects.size() + stderrOverwriteRedirects.size();
        int stderrFds[stderrNumOutputs];
        bool stderrGoesToStdout = false;

        int i = 0;
        // Open the overwrite files.
        for (; i < stderrOverwriteRedirects.size(); i++)
        {
            if(stderrOverwriteRedirects[i].empty())
            {
                stderrFds[i] = dup(STDERR_FILENO);
            }
            else
            if(stderrOverwriteRedirects[i] == "&1")
            {
                stderrGoesToStdout = true;
                stderrFds[i] = -1;
            }
            else
            {
                stderrFds[i] = open(stderrOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666);
                if (stderrFds[i] == -1)
                {
                    std::cerr << "Redirect overwrite failed for: [" << stderrOverwriteRedirects[i] << "] " << explain_open(stderrOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666) << std::endl;
                    exit(1);
                }
            }
        }

        // Open the append files
        for(int appCtr = 0; appCtr < stderrAppendRedirects.size(); appCtr++, i++)
        {
            if(stderrAppendRedirects[i].empty())
            {
                stderrFds[i] = dup(STDERR_FILENO);
            }
            else
            if(stderrAppendRedirects[i] == "&1")
            {
                stderrGoesToStdout = true;
                stderrFds[i] = -1;
            }
            else
            {
                stderrFds[i] = open(stderrAppendRedirects[appCtr].c_str(), O_WRONLY | O_APPEND, 0666);
                if (stderrFds[i] == -1)
                {
                    std::cerr << "Redirect append failed for: [" << stderrAppendRedirects[appCtr] << "]: " << explain_open(stderrOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666) << std::endl;
                    exit(1);
                }
            }
        }


        // Create an array of output files for stdout
        const int stdoutNumOutputs = stdoutAppendRedirects.size() + stdoutOverwriteRedirects.size();
        int stdoutFds[stdoutNumOutputs];
        bool stdoutGoesToStderr = false;

        i = 0;
        // Open the overwrite files.
        for (; i < stdoutOverwriteRedirects.size(); i++)
        {
            if(stdoutOverwriteRedirects[i].empty())
            {
                stdoutFds[i] = dup(STDOUT_FILENO);
            }
            else
            if(stdoutOverwriteRedirects[i] == "&2")
            {
                stdoutGoesToStderr = true;
                stdoutFds[i] =-1;
            }
            else
            {
                stdoutFds[i] = open(stdoutOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666);
                if (stdoutFds[i] == -1)
                {
                    std::cerr << "Redirect overwrite failed for: [" << stdoutOverwriteRedirects[i] << "] " << explain_open(stdoutOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666) << std::endl;
                    exit(1);
                }
            }
        }

        // Open the append files
        for(int appCtr = 0; appCtr < stdoutAppendRedirects.size(); appCtr++, i++)
        {
            if(stdoutAppendRedirects[i].empty())
            {
                stdoutFds[i] = dup(STDOUT_FILENO);
            }
            else
            if(stdoutAppendRedirects[i] == "&2")
            {
                stdoutGoesToStderr = true;
                stdoutFds[i] = -1;
            }
            else
            {
                stdoutFds[i] = open(stdoutAppendRedirects[appCtr].c_str(), O_WRONLY | O_APPEND, 0666);
                if (stdoutFds[i] == -1)
                {
                    std::cerr << "Redirect append failed for: [" << stdoutAppendRedirects[appCtr] << "]: " << explain_open(stdoutOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666) << std::endl;
                    exit(1);
                }
            }
        }

        // Redirect the output of the command to the specified files.
        execute(command, stdoutFds, stdoutNumOutputs, stderrFds, stderrNumOutputs, stdinInputFile, stderrGoesToStdout, stdoutGoesToStderr);
    }
    return 0;
}
