#include <utils.h>

#include <plugin.h>

#include <iostream>
#include <map>
#include <regex>

#include <unistd.h>

/**
 * @brief plugins are the plugins that will be returned by the plugin list provider method
 */
std::vector<PluginDescriptor> plugins = {
    PluginDescriptor{PluginClass::PLUGIN_PROMPT, "fg_impl", "check_fg_esq_sequence"},
    PluginDescriptor{PluginClass::PLUGIN_PROMPT, "bg_impl", "check_bg_esc_sequence"},
    PluginDescriptor{PluginClass::PLUGIN_PROMPT, "user_impl", "check_user_esc_sequence"},
    PluginDescriptor{PluginClass::PLUGIN_PROMPT, "host_impl", "check_host_esc_sequence"}
};

// the current foreground colour
std::string current_fg_color;
// the current baclground color
std::string current_bg_color;
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
    current_fg_color = "white";
    current_bg_color = "black";
}

/**
 * @brief called when the plugin is uninitialized
 */
extern "C" void destroy()
{
}


static std::string getFgANSIColor(const std::string& colorName) {
    std::map<std::string, std::string> colorMap = {
        {"black", "\033[30m"},
        {"red", "\033[31m"},
        {"green", "\033[32m"},
        {"yellow", "\033[33m"},
        {"blue", "\033[34m"},
        {"magenta", "\033[35m"},
        {"cyan", "\033[36m"},
        {"white", "\033[37m"}
    };

    auto it = colorMap.find(colorName);
    if (it != colorMap.end())
    {
        return it->second;
    }
    else
    {
        return ""; // Return empty string if color name is not found
    }
}

std::string getBgANSIColor(const std::string& colorName) {
    std::map<std::string, std::string> colorMap = {
        {"black", "\033[40m"},
        {"red", "\033[41m"},
        {"green", "\033[42m"},
        {"yellow", "\033[43m"},
        {"blue", "\033[44m"},
        {"magenta", "\033[45m"},
        {"cyan", "\033[46m"},
        {"white", "\033[47m"}
    };

    auto it = colorMap.find(colorName);
    if (it != colorMap.end()) {
        return it->second;
    } else {
        return ""; // Return empty string if color name is not found
    }
}

/**
 * @brief The implementation of the "\f" escape sequence to set the foreground color
 */
extern "C" std::string fg_impl()
{
    return getFgANSIColor(current_fg_color);
}

/**
 * @brief The implementation of the "\f" escape sequence to set the background color
 */
extern "C" std::string bg_impl()
{
    return getBgANSIColor(current_bg_color);
}

std::string retrieveColorValue(const std::string& input) {
    std::regex pattern("\\\\(f|b)\\{(black|red|green|yellow|blue|magenta|cyan|white)\\}");
    std::smatch match;
    if (std::regex_search(input, match, pattern))
    {
        std::string value = match[2].str();
        return value;
    }
    else
    {
        return "";
    }
}

extern "C" bool check_fg_esq_sequence(const std::string& esc_sequence)
{
    std::string s = esc_sequence;
    s = strim(s);
    if(s.rfind("\\f", 0) == 0)
    {
        std::string newFgColor = retrieveColorValue(esc_sequence);
        if(!newFgColor.empty())
        {
            current_fg_color = newFgColor;
            return true;
        }
    }

    return false;
}

extern "C" bool check_bg_esc_sequence(const std::string& esc_sequence)
{
    std::string s = esc_sequence;
    s = strim(s);
    if(s.rfind("\\b", 0) == 0)
    {
        std::string newBgColor = retrieveColorValue(esc_sequence);
        if(!newBgColor.empty())
        {
            current_bg_color = newBgColor;
            return true;
        }
    }

    return false;
}

extern "C" bool check_user_esc_sequence(const std::string& esc_sequence)
{
    std::string s = esc_sequence;
    s = strim(s);
    if(s == "\\u")
    {
        return true;
    }

    return false;
}

extern "C" std::string user_impl()
{
    std::string username = getlogin();
    return username;
}


extern "C" bool check_host_esc_sequence(const std::string& esc_sequence)
{
    std::string s = esc_sequence;
    s = strim(s);
    if(s == "\\h")
    {
        return true;
    }

    return false;
}

extern "C" std::string host_impl()
{
    char hostname[1024];
    gethostname(hostname, 1024);
    std::string shost = hostname;
    return shost;
}
