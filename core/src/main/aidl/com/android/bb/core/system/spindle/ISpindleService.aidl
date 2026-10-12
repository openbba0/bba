package com.android.bb.core.system.spindle;

import com.android.bb.core.system.spindle.ISpindleClient;

interface ISpindleService {
    boolean register(String name, ISpindleClient client);
    void emitLog(String text);
}
