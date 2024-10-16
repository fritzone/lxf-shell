#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <dlfcn.h>
#include <libgen.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <libexplain/execvp.h>
#include <libexplain/dup2.h>
#include <libexplain/pipe.h>
#include <libexplain/open.h>
#include <libexplain/fopen.h>
#include <libexplain/fork.h>

#include <sqlite3.h>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <vector>
#include <set>
#include <fstream>
#include <filesystem>

#include <utils.h>

#include "plugins/plugin.h"

/**
 * @brief getExecutablePath returns the path of the executable
 */
static std::string getExecutablePath ()
{
    char result[ PATH_MAX ] = {0};
    ssize_t count = readlink( "/proc/self/exe", result, PATH_MAX );
    (void)count;
    return std::string{dirname(result)};
}


/**
 * @brief getPromptConfig returns the path of the prompt configuration file
 */
static std::string getPromptConfig()
{
    return getExecutablePath() + "/lxf-shell-config.json";
}

/**
 * @brief getHistoryDatabase() returns the path of the history database file
 */
static std::string getHistoryDatabase()
{
    return getExecutablePath() + "/lxf-shell-history.db";
}

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

#ifdef PLUGIN_LOAD_DEBUG
        std::cout << pd.functionName << std::endl;
#endif

        //create the specific plugin for each of the plugin descriptors
        switch(pd.type)
        {
        case PluginClass::PLUGIN_COMMAND:
        {
            std::shared_ptr<CommandPlugin> commandPlugin = std::make_shared<CommandPlugin>(pd);
            commandPlugin->handler = (CommandPlugin::CFUNC_PTR*)dlsym(handle, pd.functionName.c_str());
            if(!pd.acceptanceChecker.empty())
            {
                commandPlugin->accepter = (PLUGIN_CHECK_ACCEPTANCE_FPTR*)dlsym(handle, pd.acceptanceChecker.c_str());
            }
            createdPlugins.push_back(commandPlugin);
            break;
        }
        case PluginClass::PLUGIN_PROMPT:
        {
            std::shared_ptr<PromptPlugin> promptPlugin = std::make_shared<PromptPlugin>(pd);
            promptPlugin->handler = (PromptPlugin::CFUNC_PTR*)dlsym(handle, pd.functionName.c_str());
            if(!pd.acceptanceChecker.empty())
            {
                promptPlugin->accepter = (PLUGIN_CHECK_ACCEPTANCE_FPTR*)dlsym(handle, pd.acceptanceChecker.c_str());
            }
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

std::string retrieveCurrentPrompt()
{
    static const std::string default_prompt = "lxfsh$";

    using json = nlohmann::json;

    std::ifstream file(getPromptConfig());
    if (!file)
    {
        return default_prompt;
    }

    // Parse the JSON content
    json jsonData;
    try
    {
        file >> jsonData;
    }
    catch (json::parse_error& e)
    {
        return default_prompt;
    }

    try
    {
        std::string current_prompt_value = jsonData["shell"]["prompt"]["current"];
        return current_prompt_value;
    }
    catch(json::exception& e)
    {
        return default_prompt;
    }

    return default_prompt;
}

std::vector<std::string> breakUpPrompt(const std::string& s)
{
    std::vector<std::string> result;

    size_t i = 0;
    while(i < s.length())
    {
        std::string currentNonEscapeSequence = "";
        while(i < s.length() && s[i] != '\\' )
        {
            currentNonEscapeSequence += s[i];
            i++;
        }

        if(!currentNonEscapeSequence.empty())
        {
            result.push_back(currentNonEscapeSequence);
        }

        // skip the "\"
        i++;
        if(i < s.length())
        {
            std::string currentEscapeToken = "\\";
            currentEscapeToken += s[i];

            //see if the current escapeToke is followed by the {, indicating a parameter to it
            i ++;
            if(i < s.length() && s[i] == '{')
            {
                while(i < s.length() && s[i] != '}')
                {
                    currentEscapeToken += s[i];
                    i++;
                }
                currentEscapeToken += "}";
                i++;
            }
            result.push_back(currentEscapeToken);
        }
    }
    return result;
}

/**
 * @brief readKeySequence will read the currently available key sequence from stdin
 * @return
 */
std::vector<uint8_t> readKeySequence()
{
    std::vector<uint8_t> result;

    uint8_t buffer[256] = {0};
    int size = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    if (size == -1)
    {
        return {};
    }
    else
    {
        for (int i = 0; i < size; ++i)
        {
            result.push_back(buffer[i]);
        }
#ifdef KEYSEQUENCE_DEBUG
        for(int i=0; i<result.size(); i++)
        {
            std::cout << (int)result[i] << " ";
        }
#endif
        return result;
    }
}

/**
 * @brief getTerminalSize Retrieves the terminal size
 * @param width
 * @param height
 * @return
 */
bool getTerminalSize(int& width, int& height)
{
    struct winsize w;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        return false;
    }

    width = w.ws_row;
    height = w.ws_col;
    return true;
}

/**
 * @brief enableRawMode Enables the RAW mode for the given terminal
 * @param orig_termios
 */
void enableRawMode(termios &orig_termios)
{
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/**
 * @brief disableRawMode Disables the raw mode for the given terminal
 * @param orig_termios
 */
void disableRawMode(struct termios &orig_termios)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

/**
 * @brief getCursorPos Returns the position of the cursor
 * @param x
 * @param y
 * @return
 */
bool getCursorPos(int &x, int &y)
{
    char buf[30]={0};
    int ret, i, pow;
    char ch;

    x = y = 0;

    struct termios term, restore;

    tcgetattr(0, &term);
    tcgetattr(0, &restore);
    term.c_lflag &= ~(ICANON|ECHO);
    tcsetattr(0, TCSANOW, &term);

    write(STDOUT_FILENO, "\033[6n", 4);

    for( i = 0, ch = 0; ch != 'R'; i++ )
    {
        ret = read(STDIN_FILENO, &ch, 1);
        if ( !ret )
        {
            tcsetattr(0, TCSANOW, &restore);
            return false;
        }
        buf[i] = ch;
    }

    if (i < 2)
    {
        tcsetattr(0, TCSANOW, &restore);
        return false;
    }

    for( i -= 2, pow = 1; buf[i] != ';'; i--, pow *= 10)
    {
        x = x + ( buf[i] - '0' ) * pow;
    }

    for( i-- , pow = 1; buf[i] != '['; i--, pow *= 10)
    {
        y = y + ( buf[i] - '0' ) * pow;
    }

    tcsetattr(0, TCSANOW, &restore);
    return true;
}



// Function to execute an SQL command without fetching results
void executeSQL(sqlite3* db, const std::string& sql) {
    char* errorMessage = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errorMessage);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL Error: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to execute SQL statement");
    }
}

// Function to create the database schema if it does not exist
void createHistoryDatabase(sqlite3* db) {
    const std::string createTablesSQL =
        "CREATE TABLE IF NOT EXISTS command ("
        "  id INTEGER PRIMARY KEY,"
        "  command VARCHAR(255)"
        ");"
        "CREATE TABLE IF NOT EXISTS location ("
        "  id INTEGER PRIMARY KEY,"
        "  location VARCHAR(255)"
        ");"
        "CREATE TABLE IF NOT EXISTS command_location ("
        "  created_at DATETIME,"
        "  command_id INTEGER,"
        "  location_id INTEGER,"
        "  PRIMARY KEY (created_at),"
        "  FOREIGN KEY (command_id) REFERENCES command (id),"
        "  FOREIGN KEY (location_id) REFERENCES location (id)"
        ");";

    executeSQL(db, createTablesSQL); // Create tables if not existing
}


// Function to execute an SQL query and return results with a callback
int executeQuery(sqlite3* db, const std::string& sql, int (*callback)(void*, int, char**, char**), void* data) {
    char* errorMessage = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), callback, data, &errorMessage);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL Error: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to execute SQL query");
    }

    return rc;
}

// Callback function to extract command and location from the query result
int commandLocationCallback(void* resultTuple, int argc, char** argv, char** colNames) {
    if (argc != 2) {
        throw std::runtime_error("Unexpected number of columns");
    }

    std::tuple<std::string, std::string>* result = static_cast<std::tuple<std::string, std::string>*>(resultTuple);

    // Store the command and location into the tuple
    std::get<0>(*result) = argv[0] ? argv[0] : ""; // command
    std::get<1>(*result) = argv[1] ? argv[1] : ""; // location

    return 0; // Success
}

// Function to retrieve a command-location pair from command_location table based on a given counter
std::tuple<std::string, std::string> getCommandLocation(sqlite3* db, int counter) {
    std::tuple<std::string, std::string> result;

    // SQL query to get the desired row from command_location and join with command and location
    std::string query =
        "SELECT command.command, location.location "
        "FROM command_location "
        "JOIN command ON command_location.command_id = command.id "
        "JOIN location ON command_location.location_id = location.id "
        "ORDER BY command_location.created_at DESC "
        "LIMIT 1 OFFSET " + std::to_string(counter) + ";"; // Get the row by counter as offset

    executeQuery(db, query, commandLocationCallback, &result);

    return result;
}


// Function to retrieve a command-location pair from command_location table based on a given counter and a specific location
std::tuple<std::string, std::string> getCommandLocation(sqlite3* db, int counter, const std::string& locationName) {
    std::tuple<std::string, std::string> result;

    // First, find the location ID that matches the given location name
    std::string findLocationIdQuery = "SELECT id FROM location WHERE location = '" + locationName + "';";
    int locationId = -1;
    auto locationIdCallback = [](void* idPtr, int argc, char** argv, char** azColName) -> int {
        if (argc == 1) {
            *static_cast<int*>(idPtr) = std::stoi(argv[0]);
        }
        return 0;
    };

    executeQuery(db, findLocationIdQuery, locationIdCallback, &locationId);

    if (locationId == -1) {
        std::cerr << "No location found with the name: " << locationName << std::endl;
        throw std::runtime_error("Location not found");
    }

    // SQL query to get the desired row from command_location and join with command and location, filtering by the location ID
    std::string query =
        "SELECT command.command, location.location "
        "FROM command_location "
        "JOIN command ON command_location.command_id = command.id "
        "JOIN location ON command_location.location_id = location.id "
        "WHERE location.id = " + std::to_string(locationId) + " "
                                       "ORDER BY command_location.created_at DESC "
                                       "LIMIT 1 OFFSET " + std::to_string(counter) + ";"; // Get the row by counter as offset

    executeQuery(db, query, commandLocationCallback, &result);

    return result;
}

// Helper function to get the ID from a table if a record exists, or return -1 if it doesn't
int getID(sqlite3* db, const std::string& table, const std::string& column, const std::string& value) {
    sqlite3_stmt* stmt = nullptr;
    std::string query = "SELECT id FROM " + table + " WHERE " + column + " = ?;";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) { // If there's a result, fetch the ID
        id = sqlite3_column_int(stmt, 0); // Get the first column (ID)
    }
    sqlite3_finalize(stmt);
    return id;
}

// Function to insert or fetch the command and location, then link them in command_location
void insertOrFetchCommandLocation(sqlite3* db, const std::string& command, const std::string& location) {
    sqlite3_stmt* stmt = nullptr;

    try {
        // Start a transaction
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        // Check if the command exists, otherwise insert and fetch ID
        int command_id = getID(db, "command", "command", command);
        if (command_id == -1) { // Command does not exist, insert it
            const char* insertCommandSQL = "INSERT INTO command (command) VALUES (?);";
            sqlite3_prepare_v2(db, insertCommandSQL, -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, command.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            command_id = sqlite3_last_insert_rowid(db); // Get the new ID
        }

        // Check if the location exists, otherwise insert and fetch ID
        int location_id = getID(db, "location", "location", location);
        if (location_id == -1) { // Location does not exist, insert it
            const char* insertLocationSQL = "INSERT INTO location (location) VALUES (?);";
            sqlite3_prepare_v2(db, insertLocationSQL, -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, location.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            location_id = sqlite3_last_insert_rowid(db); // Get the new ID
        }

        // Insert into command_location with the corresponding command_id and location_id
        const char* insertCommandLocationSQL =
            "INSERT INTO command_location (created_at, command_id, location_id) "
            "VALUES (datetime('now'), ?, ?);";
        sqlite3_prepare_v2(db, insertCommandLocationSQL, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, command_id); // command_id
        sqlite3_bind_int(stmt, 2, location_id); // location_id
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // Commit the transaction
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    } catch (const std::exception& ex) {
        // If there is an error, rollback the transaction
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        std::cerr << "Transaction failed: " << ex.what() << std::endl;
        throw;
    }
}

/**
 * @brief storeCommandHistory Will store the current location and the given command in the history database
 * @param command the command to store
 * @return
 */
bool storeCommandHistory(const std::string& command)
{
    namespace fs = std::filesystem;

    sqlite3* db = nullptr;
    int rc = sqlite3_open(getHistoryDatabase().c_str(), &db); // Open or create the database

    if (rc)
    {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    try
    {
        // Insert a command and a location, or fetch existing IDs, then link them in command_location
        insertOrFetchCommandLocation(db, command, fs::current_path());
    }
    catch (const std::exception& ex)
    {
        std::cerr << "An error occurred: " << ex.what() << std::endl;
        return false;
    }

    sqlite3_close(db); // Close the database
    return true;
}


static const std::map<const char*, std::vector<uint8_t>> reservedKeys
    {
     {"Up", {27, 91, 65}},
     {"Down", {27, 91, 66}},
     {"CtrlUp", {27, 91, 49, 59, 53, 65}},
     {"CtrlDown", {27, 91, 49, 59, 53, 66}},
     };

static int currentHistoryElementIndex = 0;

std::string resolveHistory(const std::string& shortcut)
{

    if(shortcut == "Up")
    {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(getHistoryDatabase().c_str(), &db); // Open or create the database

        if (rc)
        {
            std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
            return "";
        }

        auto prevCommand = getCommandLocation(db, currentHistoryElementIndex);

        sqlite3_close(db);

        currentHistoryElementIndex ++;

        return std::get<0>(prevCommand);
    }


    if(shortcut == "CtrlUp")
    {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(getHistoryDatabase().c_str(), &db); // Open or create the database

        if (rc)
        {
            std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
            return "";
        }

        auto prevCommand = getCommandLocation(db, currentHistoryElementIndex, std::filesystem::current_path());

        sqlite3_close(db);

        currentHistoryElementIndex ++;

        return std::get<0>(prevCommand);
    }

    return "";
}

/**
 * @brief main Main entry point
 * @return
 */
int main()
{

    // set up the database for the command history
    sqlite3* db;

    int rc = sqlite3_open(getHistoryDatabase().c_str(), &db);
    if (rc)
    {
        std::cerr << "Can't open database: " << getHistoryDatabase() << ". " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    try
    {
        createHistoryDatabase(db);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "An error occurred: " << ex.what() << std::endl;
    }

    sqlite3_close(db);

    // let's gather the plugins
    std::filesystem::path currentPath = std::filesystem::current_path();
    std::filesystem::path pluginDirectory = currentPath / "plugins";

    auto pluginContainers = listPluginFiles(pluginDirectory);

    while(true)
    {
        std::string currentPromptElements = retrieveCurrentPrompt();
        auto promptElements = breakUpPrompt(currentPromptElements);
        std::string currentPrompt;

        for(const auto& promptElement : promptElements)
        {
            bool currentElementAccepted = false;
            if(promptElement.length() > 0 && promptElement[0] == '\\')
            {
                for(const auto& pc : pluginContainers)
                {
                    for(const auto& p : pc.plugins)
                    {
                        if(p->getClass() == PluginClass::PLUGIN_PROMPT)
                        {
                            // now let's break up the prompts' value into its elements and
                            // see if this plugin accepts it
                            if((dynamic_cast<PromptPlugin&&>(*p)).accepter)
                            {
                                bool accepts = (dynamic_cast<PromptPlugin&&>(*p)).accepter(promptElement);
                                if(accepts)
                                {
                                    std::string promptPluginReturns = (dynamic_cast<PromptPlugin&&>(*p)).handler();
                                    currentPrompt += promptPluginReturns;
                                    currentElementAccepted = true;
                                }
                            }
                        }
                        if(currentElementAccepted)
                        {
                            break;
                        }
                    }
                    if(currentElementAccepted)
                    {
                        break;
                    }
                }
            }

            if(!currentElementAccepted)
            {
                currentPrompt += promptElement;
            }
        }


        // The command string
        std::string command = "";

        // This enclosed block will be the reading of the command, that was getline till now
        {
        // Enabling the raw mode for the current terminal
        struct termios orig_termios;
        tcgetattr(STDIN_FILENO, &orig_termios);
        enableRawMode(orig_termios);

        fd_set readfds_orig = {0};
        FD_SET(0, &readfds_orig);
        int max_fd = 0;

        // main loop for reading the command
        while(true)
        {

            // send cursor to first position:
            std::cout << "\r";

            // erase current line
            std::cout << "\x1b[2K";
            fflush(stdout);

            // print the current prompt
            if(currentPrompt.empty())
            {
                std::cout << "\rlxfsh$ " << command;
            }
            else
            {
                std::cout << "\r" << currentPrompt  << "\e[0m" << command;
            }


            // need this to print on the terminal, because we are in raw mode explicit call to fflush(stdout) is required
            fflush(stdout);

            fd_set readfds = readfds_orig;
            if (select(max_fd + 1, &readfds, nullptr, nullptr, nullptr) != -1 && FD_ISSET(0, &readfds))
            {
                // stdin is ready for read()
                auto keys = readKeySequence();

                // if we pressed the Enter key, that will give one byte back, with the value 10.
                if(keys.size() == 1 && keys[0] == 10)
                {
                    break;
                }

                // Just a simple backspace handler, so that we don't have ot re-type
                if(keys.size() == 1 && keys[0] == 127)
                {
                    command.pop_back();
                }

                if(keys.size() == 1)
                {
                    if(isprint(keys[0]) || isspace(keys[0])) // this is a normal character that can go in a command
                    {
                        command += static_cast<char>(keys[0]);
                    }
                }
                else // let's see if this is a key sequence or not
                {
                    std::string shortcut = "";
                    // Iterate over all key-value pairs in the map
                    for (const auto& kv : reservedKeys)
                    {
                        // If the input vector matches the current map value, return the key
                        if (kv.second == keys)
                        {
                            shortcut = kv.first; // return the string key corresponding to the match
                            break;
                        }
                    }

                    if(!shortcut.empty())
                    {
                        command = resolveHistory(shortcut);
                    }
                }
            }
        }

        disableRawMode(orig_termios);

        }

        // to print a newline after the command input
        std::cout << std::endl;
        fflush(stdout);

        // and store the command in the command database, regardless if it was executed successfully or not
        storeCommandHistory(command);

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
