#include "sys_tools.h"

#include <libgen.h>
#include <unistd.h>

#include <linux/limits.h>

std::string getExecutablePath ()
{
    char result[ PATH_MAX ] = {0};
    ssize_t count = readlink( "/proc/self/exe", result, PATH_MAX );
    (void)count;
    return std::string{dirname(result)};
}
