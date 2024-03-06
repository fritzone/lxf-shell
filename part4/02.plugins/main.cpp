#include <unistd.h>
#include <fcntl.h>

#include <libexplain/execvp.h>
#include <libexplain/dup2.h>
#include <libexplain/pipe.h>
#include <libexplain/open.h>
#include <libexplain/fopen.h>
#include <libexplain/fork.h>

#include <sys/wait.h>
#include <sys/types.h>

#include <dlfcn.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <vector>
#include <set>
#include <filesystem>

#include <utils.h>
#include "plugins/plugin.h"

/**
 * @brief execute executes the given program.
 *
 * The program comes in the form of <appname> [parameters]. The function separates these into valid data.
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
            int* stdoutFds, size_t numStdoutFds,
            int* stderrFds, size_t numStderrFds,
            const std::string& stdinInputFile,
            bool stderrGoesToStdout, bool stdoutGoesToStderr,
            int stdinFileNo, int stdoutFileNo, int stderrFileNo)
{
    const bool needsStdoutRedirect = numStdoutFds > 0 || stdoutFileNo != STDOUT_FILENO;        // do we need to redirect stdout?
    const bool needsStderrRedirect = numStderrFds > 0 || stderrFileNo != STDERR_FILENO;        // do we need to redirect stderr?
    const bool needsStdinRedirect = ! stdinInputFile.empty() || stdinFileNo != STDIN_FILENO; // If there is a stdin file, we open it, otherwise no stdin redirect
    int pipeStdoutFd[2] = {-1, -1};                     // Pipe for stdout
    int pipeStderrFd[2] = {-1, -1};                     // Pipe for stderr
    int pipeStdinFd[2] =  {-1, -1};                     // Pipe for stdin

    if(needsStdoutRedirect)
    {
        // Create a pipe to capture the command's output.
        if (pipe(pipeStdoutFd) == -1)
        {
            log_err << explain_pipe(pipeStdoutFd) << std::endl;
            return 1;
        }
    }

    if(needsStderrRedirect)
    {
        // Create a pipe to capture the command's output.
        if (pipe(pipeStderrFd) == -1)
        {
            log_err << explain_pipe(pipeStderrFd) << std::endl;
            return 1;
        }
    }

    if(needsStdinRedirect)
    {
        // Create a pipe for the stdin redirection
        if (pipe(pipeStdinFd) == -1)
        {
            log_err << explain_pipe(pipeStdinFd) << std::endl;
            return 1;
        }
    }

    // fork the process

    if (const pid_t pid = fork(); pid == -1)
    {
        log_err << explain_fork() << std::endl;
        return 1;
    }
    else
    if (pid == 0) { // Child process

        // Set up the STDOUT redirect
        if(needsStdoutRedirect)
        {
            closeFd(pipeStdoutFd[0]);

            // Redirect stdout to the pipe.
            if(dup2(pipeStdoutFd[1], stdoutFileNo) == -1)
            {
                log_err << "Cannot dup2 stdout:" << explain_dup2(pipeStdoutFd[1], stdoutFileNo ) << std::endl;
                return 1;
            }

            // closeFd the original file descriptors.
            closeFd(pipeStdoutFd[1]);
            for (int i = 0; i < numStdoutFds; i++)
            {
                if(stdoutFds[i] != -1)
                {
                    closeFd(stdoutFds[i]);
                }
            }
        }

        // Set up the STDERR redirect
        if(needsStderrRedirect)
        {
            closeFd(pipeStderrFd[0]);

            // Redirect stderr to the pipe.
            if(dup2(pipeStderrFd[1], stderrFileNo) == -1)
            {
                log_err << "Cannot dup2 stderr: " << explain_dup2(pipeStdoutFd[1], stderrFileNo) << std::endl;
                return 1;
            }

            // closeFd the original file descriptors.
            closeFd(pipeStderrFd[1]);
            for (int i = 0; i < numStderrFds; i++)
            {
                if(stderrFds[i] != -1)
                {
                    closeFd(stderrFds[i]);
                }
            }
        }

        if(needsStdinRedirect)
        {
            // closeFd the write end of the input pipe
            closeFd(pipeStdinFd[1]);

            // Redirect stdin to read from the input pipe
            if(dup2(pipeStdinFd[0], stdinFileNo) == -1)
            {
                log_err << "Cannot dup2 stdin:" << explain_dup2(pipeStdinFd[0], stdinFileNo) << std::endl;
            }
            closeFd(pipeStdinFd[0]);
        }

        // Here we start the execution of the command

        // First: let's split the command into separate tokens
        const auto split = splitStringByWhitespace(program);

        // Then create the arguments array
        const char** arguments = new const char*[split.size() + 1];
        std::unique_ptr<const char*> r;
        r.reset(arguments);

        {
        size_t i = 0;
        for(; i<split.size(); i++)
        {
            arguments[i] = split[i].c_str();
        }
        arguments[i] = nullptr;
        }

        if (execvp(arguments[0], const_cast<char * const*>(arguments)) == -1)
        {
            closeFd(pipeStdoutFd[1]);
            closeFd(pipeStderrFd[1]);
            closeFd(pipeStdoutFd[0]);
            closeFd(pipeStderrFd[0]);

            for (int i = 0; i < numStdoutFds; i++)
            {
                if(stdoutFds[i] != -1)
                {
                    closeFd(stdoutFds[i]);
                }
            }

            for (int i = 0; i < numStderrFds; i++)
            {
                if(stderrFds[i] != -1)
                {
                    closeFd(stderrFds[i]);
                }
            }
            log_err << explain_execvp(program.c_str(), const_cast<char * const*>(arguments)) << std::endl;
            return 1;
        }
    }
    else // Parent process
    {

        if(needsStdinRedirect)
        {
            closeFd(pipeStdinFd[0]); // closeFd the read end of the input pipe

            if(stdinInputFile.empty())
            {
                char buffer[4096] = {0};
                ssize_t bytes_read;

                // Read the output from the pipe and write it to the inout pipe.
                while ((bytes_read = read(stdinFileNo, buffer, sizeof(buffer))) > 0)
                {
                    write(pipeStdinFd[1], buffer, bytes_read);
                }
            }
            else
                {
                // Open the input file to be used
                FILE *input_file = fopen(stdinInputFile.c_str(), "r");
                if (input_file == nullptr)
                {
                    log_err << explain_fopen(stdinInputFile.c_str(), "r") << std::endl;
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
            }
            closeFd(pipeStdinFd[1]);
        }

        if(needsStdoutRedirect)
        {
            // closeFd the write end of the pipe since we're not using it.
            closeFd(pipeStdoutFd[1]);

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

            // closeFd the pipe and wait for the child process to finish.
            closeFd(pipeStdoutFd[0]);
        }

        if(needsStderrRedirect)
        {
            // closeFd the write end of the pipe since we're not using it.
            closeFd(pipeStderrFd[1]);

            // Create a buffer to capture the errput from the pipe.
            char buffer[4096] = {0};
            ssize_t bytes_read;

            // Read the error output from the pipe and write it to the error output files.
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

            // closeFd the pipe and wait for the child process to finish.
            closeFd(pipeStderrFd[0]);
        }

        for (int i = 0; i < numStdoutFds; i++)
        {
            if(stdoutFds[i] != -1)
            {
                closeFd(stdoutFds[i]);
            }
        }

        for (int i = 0; i < numStderrFds; i++)
        {
            if(stderrFds[i] != -1)
            {
                closeFd(stderrFds[i]);
            }
        }

        // and wait for the PID
        waitpid(pid, nullptr, 0);
    }
    return 0;
}

/**
 * @brief Will run the given string as a command, possibly with parameters and various redirections
 *
 * @param command The command to run
 *
 * @return true in case of success, false otherwise
 */
bool runAsSingleCommand(std::string command, int stdinFileNo = STDIN_FILENO, int stdoutFileNo = STDOUT_FILENO, int stderrFileNo = STDERR_FILENO)
{
    // gather all the files we redirect to
    const std::vector<std::string> stderrAppendRedirects = extractWordsAfterSequence(command, "2>>");
    const std::vector<std::string> stderrOverwriteRedirects = extractWordsAfterSequence(command, "2>");
    const std::vector<std::string> stdoutAppendRedirects = extractWordsAfterSequence(command, ">>");
    const std::vector<std::string> stdoutOverwriteRedirects = extractWordsAfterSequence(command, ">");

    // this will be the file from which we read the input redirect if any
    const std::vector<std::string> stdinInputFiles = extractWordsAfterSequence(command, "<");
    const std::string stdinInputFile = stdinInputFiles.empty() ? "" : stdinInputFiles[0];

    // Create an array of output files for stderr
    const auto stderrNumOutputs = stderrOverwriteRedirects.size() + stderrAppendRedirects.size();
    int stderrFds[stderrNumOutputs];
    bool stderrGoesToStdout = false;

    int i = 0;
    // Open the overwrite files.
    for (; i < stderrOverwriteRedirects.size(); i++)
    {
        if(stderrOverwriteRedirects[i].empty())
        {
            stderrFds[i] = dup(stderrFileNo);
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
                log_err << "Redirect overwrite failed for: [" << stderrOverwriteRedirects[i] << "] " << explain_open(stderrOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666) << std::endl;
                return false;
            }
        }
    }

    // Open the append files
    for(int appCtr = 0; appCtr < stderrAppendRedirects.size(); appCtr++, i++)
    {
        if(stderrAppendRedirects[i].empty())
        {
            stderrFds[i] = dup(stderrFileNo);
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
                log_err << "Redirect append failed for: [" << stderrAppendRedirects[appCtr] << "]: " << explain_open(stderrOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666) << std::endl;
                return false;
            }
        }
    }

    // Create an array of output files for stdout
    const auto stdoutNumOutputs = stdoutAppendRedirects.size() + stdoutOverwriteRedirects.size();
    int stdoutFds[stdoutNumOutputs];
    bool stdoutGoesToStderr = false;

    i = 0;
    // Open the overwrite files.
    for (; i < stdoutOverwriteRedirects.size(); i++)
    {
        if(stdoutOverwriteRedirects[i].empty())
        {
            stdoutFds[i] = dup(stdoutFileNo);
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
                log_err << "Redirect overwrite failed for: [" << stdoutOverwriteRedirects[i] << "] " << explain_open(stdoutOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666) << std::endl;
                return false;
            }
        }
    }

    // Open the append files
    for(int appCtr = 0; appCtr < stdoutAppendRedirects.size(); appCtr++, i++)
    {
        if(stdoutAppendRedirects[i].empty())
        {
            stdoutFds[i] = dup(stdoutFileNo);
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
                log_err << "Redirect append failed for: [" << stdoutAppendRedirects[appCtr] << "]: " << explain_open(stdoutOverwriteRedirects[i].c_str(), O_WRONLY | O_CREAT, 0666) << std::endl;
                return false;
            }
        }
    }

    // Redirect the output of the command to the specified files.
    execute(command, stdoutFds, stdoutNumOutputs, stderrFds, stderrNumOutputs, stdinInputFile,
        stderrGoesToStdout, stdoutGoesToStderr, stdinFileNo, stdoutFileNo, stderrFileNo);
    return true;
}

/**
 * @brief Will run each of the commands in the vector in a way that the output of the current command
 * will be used as input for the next command, like the piping in Linux shell works
 *
 * @param commands the commands
 *
 * \return true in case of success, false otherwise
 */
bool runPipedCommands(const std::vector<std::string>& commands)
{
    const std::size_t numPipes = commands.size();

    int pipefds[2 * numPipes] = {-1};

    for(int i = 0; i < numPipes; i++)
    {
        if(pipe(pipefds + i*2) < 0)
        {
            log_err << explain_pipe(pipefds + i*2) << std::endl;
            return false;
        }
    }

    auto cmd = commands.begin();

    int j = 0;
    while(cmd != commands.end())
    {
        if(const pid_t pid = fork(); pid == 0)
        {
            //check if we are not running the last command
            auto nextCommand = cmd;
            ++ nextCommand;

            if(nextCommand != commands.end())
            {
                if((dup2(pipefds[j + 1], STDOUT_FILENO)) == -1)
                {
                    log_err << "Cannot dup2 stdout in piper:" << explain_dup2(pipefds[j-2], 0);
                    return false;
                }
            }

            if(j != 0 )
            {
                if((dup2(pipefds[j-2], STDIN_FILENO)) == -1)
                {
                    log_err << "Cannot dup2 stdin in piper:" << explain_dup2(pipefds[j-2], 0);
                    return false;
                }
            }

            for(int i = 0; i < 2*numPipes; i++)
            {
                closeFd(pipefds[i]);
            }

            // First: let's split the command into separate tokens
            auto split = splitStringByWhitespace(*cmd);

            // Then create the arguments array
            const char** arguments = new const char*[split.size() + 1];
            std::unique_ptr<const char*> r;
            r.reset(arguments);

            for(size_t i = 0; i<split.size(); i++)
            {
                arguments[i] = split[i].c_str();
            }
            arguments[split.size()] = nullptr;

            if (execvp(arguments[0], const_cast<char * const*>(arguments)) == -1)
            {
                log_err << explain_execvp(arguments[0], const_cast<char * const*>(arguments)) << std::endl;
            }
        }
        else
        if(pid < 0)
        {
            log_err << explain_fork() << std::endl;
            return false;
        }

        ++ cmd;
        j+=2;
    }

    for(int i = 0; i < 2 * numPipes; i++)
    {
        closeFd(pipefds[i]);
    }

    for(int i = 0; i < numPipes + 1; i++)
    {
        int status = -1;
        waitpid(-1, &status, 0);
    }
    return true;
}

/**
 * @brief createPluginContainer This method will load all the plugins from the given shared library.
 *
 * @param path the name of the shared library
 *
 * @return the properly constructed container with the plugin descriptors for all the different kind of plugins
 */
static PluginContainer createPluginContainer(const std::string& path)
{
    // the handle to load the library
    void *handle = nullptr;

    // the various function pointers
    void (*initialize)(void*);  // the one to initialize the library
    std::vector<PluginDescriptor> (*providePlugins)(PluginClass);// the one to load the plugins

    // Let's load the shared library from the given path
    handle = dlopen (path.c_str(), RTLD_LAZY);
    if(!handle)
    {
        std::cerr << "Cannot open plugin " << path << ": " << dlerror() << std::endl;
        return {};
    }

    // first: fetch the "initialize" method from the plugin and execute if possible
    *(void **)(&initialize) = dlsym(handle, "initialize");
    if(initialize)
    {
        // let's call the initialize function of the plugin
        initialize(nullptr);
    }

    // now load all the plugin functions
    *(void **)(&providePlugins) = dlsym(handle, "providePlugins");
    if(!providePlugins)
    {
        std::cerr << "Cannot use plugin " << path << ": " << dlerror() << std::endl;
        return {};
    }

    std::vector<std::shared_ptr<PluginBase>> createdPlugins;
    std::vector<PluginDescriptor> plugins = providePlugins(PluginClass::PLUGIN_ALL);
    for(const auto& pd : plugins)
    {
        //create the specific plugin for each of the plugin descriptors
        switch(pd.type)
        {
        case PluginClass::PLUGIN_COMMAND:
        {
            std::shared_ptr<CommandPlugin> commandPlugin = std::make_shared<CommandPlugin>(pd);
            commandPlugin->handler = (CommandPlugin::CFUNC_PTR*)dlsym(handle, pd.functionName.c_str());
            createdPlugins.push_back(commandPlugin);
            break;
        }
        case PluginClass::PLUGIN_PROMPT:
        {
            std::shared_ptr<PromptPlugin> promptPlugin = std::make_shared<PromptPlugin>(pd);
            promptPlugin->handler = (PromptPlugin::CFUNC_PTR*)dlsym(handle, pd.functionName.c_str());
            createdPlugins.push_back(promptPlugin);
            break;
        }
        default:
            break;
        }
    }

    return {handle, path, createdPlugins, nullptr};
}

/**
 * @brief listPluginFiles Will create a list of plugin containers for all the shared libraries
 * that are to be
 * @param dir
 * @return
 */
std::vector<PluginContainer> listPluginFiles(const std::filesystem::path& dir)
{
    std::vector<PluginContainer> result;

    // Check if the directory exists
    if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir))
    {
        // Iterate over the entries in the directory
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
        {
            // Check if the file is a regular file and has a .so extension
            if (entry.is_regular_file() && entry.path().extension() == ".so")
            {
                auto pc = createPluginContainer( entry.path() );
                result.push_back(pc);
            }
        }
    }

    return result;
}

/**
 * @brief main Main entry point
 * @return
 */
int main()
{
    // let's gather the plugins
    std::filesystem::path currentPath = std::filesystem::current_path();
    std::filesystem::path pluginDirectory = currentPath / "plugins";

    auto pluginContainers = listPluginFiles(pluginDirectory);

    while(true)
    {
        std::string currentPrompt = "";

        for(const auto& pc : pluginContainers)
        {
            for(const auto& p : pc.plugins)
            {
                if(p->getClass() == PluginClass::PLUGIN_PROMPT)
                {
                    std::string promptPluginReturns = (dynamic_cast<PromptPlugin&&>(*p)).handler();
                    currentPrompt += promptPluginReturns;
                }
            }
        }
        if(currentPrompt.empty())
        {
            std::cout << "lxfsh$ ";
        }
        else
        {
            std::cout << currentPrompt  << "$ ";
        }

        std::string command;
        std::getline(std::cin, command);
        if(command == "exit")
        {
            exit(0);
        }

        for(const auto& pc : pluginContainers)
        {
            for(const auto& p : pc.plugins)
            {
                if(p->getClass() == PluginClass::PLUGIN_COMMAND)
                {
                    std::string commandPluginReturns = (dynamic_cast<CommandPlugin&&>(*p)).handler(command);
                    if(!commandPluginReturns.empty())
                    {
                        if(unquotedCharacter(command))
                        {
                            const auto commands = splitStringWithDelimiter(command, "|");
                            runPipedCommands(commands);
                        }
                        else
                        {
                            runAsSingleCommand(command);
                        }
                    }
                }
            }
        }
    }
}
