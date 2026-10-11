package com.android.blackbox.fake;

import com.android.jnihook.ReflectCore;


public class FakeCore {
    public static void init() {
        ReflectCore.set(android.app.ActivityThread.class);
    }
}
