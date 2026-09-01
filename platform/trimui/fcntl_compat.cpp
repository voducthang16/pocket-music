#include <fcntl.h>

#include <cerrno>
#include <cstdarg>

extern "C" int fcntl(int descriptor, int command, ...);

extern "C" int fcntl64(int descriptor, int command, ...) {
    if (command != F_SETFD) {
        errno = EINVAL;
        return -1;
    }
    va_list arguments;
    va_start(arguments, command);
    const int argument = va_arg(arguments, int);
    va_end(arguments);
    return fcntl(descriptor, command, argument);
}
