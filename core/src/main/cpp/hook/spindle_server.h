#ifndef BLACKBOX_SPINDLE_SERVER_H
#define BLACKBOX_SPINDLE_SERVER_H

// TCP listener in the :black process. Main and guest processes register a
// Binder client; this server delivers frames through BSpindleService.
class SpindleServer {
public:
    static void start();
    static int addAgent(const char *name, int pid, int uid);
    static void dropAgent(int id);
    static void hostLog(int pid, const char *text);
};

#endif
