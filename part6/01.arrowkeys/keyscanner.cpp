#include <termios.h>

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <iostream>
#include <vector>
#include <string>

static const std::vector<const char*> keys = {"Up", "Down", "Left", "Right", "CtrlUp", "CtrlDown", "CtrlLeft", "CtrlRight", "CtrlR", "Home", "End", "Enter", "Backspace", "Delete"};

static volatile sig_atomic_t got_sigwinch = 0;

static void sigwinch_handler(int sig)
{
    (void)sig;
    got_sigwinch = 1;
}

void enableRawMode(termios &orig_termios) {
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode(struct termios &orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

std::vector<uint8_t> read_stdin()
{
    char buffer[1024] = {0};
    std::vector<uint8_t> result;

    int size = read(0, buffer, sizeof(buffer) - 1);
    if (size == -1)
    {
        return {};
    }
    else
    {
        // Check for ESC
        if (size == 1 && buffer[0] == 0x1B)
        {
            return {buffer[0]};
        }

        std::cout << "size=" << size << std::endl;
        // Show the buffer contents in hex
        char byte_str_buf[8];
        for (int i = 0; i < size; ++i)
        {
            sprintf(byte_str_buf, "%02d ", buffer[i]);
            printf("%s", byte_str_buf);
            result.push_back(buffer[i]);
        }
        return result;
    }
}

int main()
{

    // Register SIGWINCH signal handler to handle resizes: select() fails on
    // resize, but we want to know if it was a resize because don't want to
    // abort on resize.
    struct sigaction sa;
    sa.sa_handler = sigwinch_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGWINCH, &sa, NULL) == -1)
    {
        std::cerr << "Can't register SIGWINCH action." << std::endl;
        exit(1);
    }

    // Initialize ncurses
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    enableRawMode(orig_termios);

    // select() setup. You usually want to add more stuff here (sockets etc.).
    fd_set readfds_orig;
    memset(&readfds_orig, 0, sizeof(fd_set));

    FD_SET(0, &readfds_orig);
    int max_fd = 0;

    fd_set* writefds = NULL;
    fd_set* exceptfds = NULL;
    struct timeval* timeout = NULL;

    // sigwinch counter, just to show how many SIGWINCHs caught.
    int sigwinchs = 0;

    // Main loop
    for (const auto keyName : keys)
    {
        std::cout << "Please press " << keyName << ":";
        fflush(stdout);

        fd_set readfds = readfds_orig;
        if (select(max_fd + 1, &readfds, writefds, exceptfds, timeout) == -1)
        {
            // Handle errors. This is probably SIGWINCH.
            if (got_sigwinch)
            {
                char sigwinch_msg[100];
                sprintf(sigwinch_msg, "got sigwinch (%d)", ++sigwinchs);
            }
            else
            {
                break;
            }
        }
        else if (FD_ISSET(0, &readfds))
        {
            // stdin is ready for read()
            auto keys = read_stdin();
            bool quit = keys.size() == 1 && keys[0] == 0x1b;
            if (quit)
            {
                break;
            }
            std::cout << std::endl;
        }
    }

    disableRawMode(orig_termios);
    return 0;
}
