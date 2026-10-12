package com.android.bb.core.system.spindle;

interface ISpindleClient {
    byte[] onFrame(in byte[] frame);
    boolean launch(String packageName);
}
