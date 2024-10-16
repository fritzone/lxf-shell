#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <string>
#include <vector>
#include <functional>
#include <memory>

/**
 * @brief The PluginClass enum presents all the possible types of plugins that can be implemented
 * in the plugin architecture.
 */
enum class PluginClass
{

    PLUGIN_ALL           = 0,   // Just a placeholder for all plugins
    PLUGIN_COMMAND       = 1,   // this will introduce a new command to the system
    PLUGIN_PROMPT        = 2,   // this will print something in the prompt
    PLUGIN_KEYHANDLER    = 3,   // this will handle a keypress
    PLUGIN_TIMER         = 4,   // this will be calledd periodically
    PLUGIN_REACTIONIST   = 5,   // this will be called when the computing environment changed

    PLUGIN_UNKNOWN       = 255  // this will not be called
};


/**
 * @brief The PluginDescriptor class represents a plugin that is implemented in this shared library
 *
 * A vector containing objects of this type is returned by the providePlugins method in order to acknowledge the
 * shell about the plugins that are implemented in this shared library.
 */
struct PluginDescriptor
{
    PluginDescriptor(PluginClass ptype, const std::string& pfunctionName):
        type(ptype), functionName(pfunctionName)
    {}

    PluginDescriptor(PluginClass ptype, const std::string& pfunctionName, const std::string& pacceptanceChecker):
        type(ptype), functionName(pfunctionName), acceptanceChecker(pacceptanceChecker)
    {}

    virtual ~PluginDescriptor() = default;

    PluginClass type;               // the type of the plugin, from the list above
    std::string functionName;       // the name of the function in the shared library which implements that functionality
    std::string acceptanceChecker;  // the name of the function in the shared library which implements the check if the given plugin accepts a specific string
};

/**
 * The following block defines the interface of the shared library. It must use "C" linkage in order
 * to have the names usable without concerning the mangling
 */
extern "C"
{
/**
 * @brief providePlugins will provide all the plugins that are declared in this shared library
 *
 * This method will return a list of PluginDescriptor structures,each of them indicating the type
 * of the plugin the corresponding method holds, and the name of the function that implements it.
 * The last element in the list must be nullptr, in order to indicate the end of it.
 *
 * The parameter pluginClass determines which class of plugins to load, for the moment is not used.
 *
 * @return a list of PluginDescriptor structures
 */
std::vector<PluginDescriptor> providePlugins(PluginClass pc = PluginClass::PLUGIN_ALL);

/**
 * @brief initialize will initialize this plugin library. Is called upon plugin initialization.
 *
 * All the necessary allocations and initializations concerning the plugins must be done in here.
 */
void initialize();

/**
 * @brief destroy Is called upon unloading the shared library.
 *
 * The necessary de-initializations and resource freeing must be done in here.
 */
void destroy();

}   // extern "C"


/**
 * This is the interface for the Command accepting function, for all type of plugins that require it.
 *
 * The method will check whether the given plugin accepts the given string as parameter it can workj with.
 *
 * It should return either true, meaning the command will be handled in the plugin
 * or false, meaning the command is not handled by this plugin.
 */
typedef bool PLUGIN_CHECK_ACCEPTANCE_FPTR(const std::string&);

/**
 * This is the interface for the Command Handler plugins, ie. type: PLUGIN_COMMAND.
 *
 * The method will get the command line received from the shell input as the parameter.
 *
 * It should return either an empty string, meaning the command was successfully handled in the plugin
 * or any other value, that will be dealt with the shell or a consecutive plugin.
 */
typedef std::string PLUGIN_COMMAND_FPTR(const std::string&);

/**
 * This is the interface for the Prompt Printer plugins, ie. type: PLUGIN_PROMPT.
 *
 * The method will be called when the shell will print the command prompt.
 *
 * It should return either an empty string, meaning the shell should print its own prompt
 * or a string, that will be used as the prompt for the shell.
 */
typedef std::string PROMPT_COMMAND_FPTR();

/**
 * @brief The PluginBase class is the common base for all the type of plugins
 * that can be implemented in the shell plugin architecture
 */
struct PluginBase
{
    virtual ~PluginBase() = default;
    virtual PluginClass getClass() const = 0;
};

/**
 * @brief The Plugin class Abstract bridge class
 */
template<typename C> struct Plugin : public PluginBase
{
    explicit Plugin(PluginDescriptor pdescriptor) : descriptor(pdescriptor)
    {}

    PluginDescriptor descriptor;

    using CFUNC_PTR = C;

    // this is the handler of this plugin
    std::function<CFUNC_PTR> handler;

    // this will check if the current plugin accepts a given string
    std::function<PLUGIN_CHECK_ACCEPTANCE_FPTR> accepter;

};

/**
 * @brief The CommandPlugin class encapsulates one plugin that handles a command
 */
struct CommandPlugin : public Plugin<PLUGIN_COMMAND_FPTR>
{
    CommandPlugin(PluginDescriptor d) : Plugin(d)
    {}

    PluginClass getClass() const {return PluginClass::PLUGIN_COMMAND;}
};

/**
 * @brief The CommandPlugin class encapsulates one plugin that handles a command
 */
struct PromptPlugin : public Plugin<PROMPT_COMMAND_FPTR>
{
    PromptPlugin(PluginDescriptor d) : Plugin(d)
    {}

    PluginClass getClass() const {return PluginClass::PLUGIN_PROMPT;}
};

/**
 * @brief The PluginContainer class encapsulates one plugin shared library.
 */
struct PluginContainer
{
    void* handle;                                               // the handle that will need to be dlclose'd
    std::string path;                                           // the actual path of the plugin shared library
    std::vector<std::shared_ptr<PluginBase>> plugins;           // the plugins loaded from the library
    std::function<void(void)> destroyer;                        // the function that will be called before dlclose
};


#endif

