#include <utils.h>

#include <plugin.h>
#include <iostream>
#include <filesystem>

/**
 * @brief plugins are the plugins that will be returned by the plugin list provider method
 */
std::vector<PluginDescriptor> plugins = {
    PluginDescriptor{PluginClass::PLUGIN_COMMAND, "cd_impl"},
    PluginDescriptor{PluginClass::PLUGIN_PROMPT, "print_cwd"}
};

// a shorthand to not to have to type all of it all the time
namespace fs = std::filesystem;

// the current directory
fs::path currentDirectory;

/**
 * @brief Provides the list of plugins implemented by this library.
 * @return the list of plugins implemented by this library
 */
extern "C" std::vector<PluginDescriptor> providePlugins(PluginClass)
{
    return plugins;
}

/**
 * @brief initialize is called when the plugin is loaded
 */
extern "C" void initialize()
{
    currentDirectory = fs::current_path();
}

/**
 * @brief called when the plugin is uninitialized
 */
extern "C" void destroy()
{
}

/**
 * @brief The implementation of the "cd" command.
 */
extern "C" std::string cd_impl(const std::string& in)
{
    std::string cmd = in;
    cmd = strim(cmd);
    if(cmd.substr(0, 2) == "cd")
    {
        // let's chop out the "cd" from it, and trim it
        cmd = cmd.substr(2);
        cmd = strim(cmd);

        std::filesystem::path directoryPath(cmd);

        // Check if the path is absolute, and if not, make it absolute relative to the current working directory
        if (!directoryPath.is_absolute())
        {
            directoryPath = std::filesystem::absolute(directoryPath);
        }

        // Set the current working directory to the absolute path
        std::filesystem::current_path(directoryPath);
        currentDirectory = fs::current_path();

        return "";
    }
    return in;
}

extern "C" std::string print_cwd()
{
    return currentDirectory.generic_string();
}
