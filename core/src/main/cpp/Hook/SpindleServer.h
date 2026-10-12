#ifndef BLACKBOX_SPINDLE_SERVER_H
#define BLACKBOX_SPINDLE_SERVER_H

// TCP spindle_server in the bba main process. Guest processes join() so a
// script from the dev machine is delivered into that client process.
class SpindleServer {
public:
    static void start();
    static void join(const char *name);
};

#endif
