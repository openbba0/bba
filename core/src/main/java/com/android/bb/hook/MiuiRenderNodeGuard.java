package com.android.bb.hook;

import com.android.bb.utils.Slog;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * HyperOS creates the first {@code View} through {@code RenderNodeStubImpl}.
 * That class loads {@code libforcedarkimpl} on the main thread and never returns,
 * so the guest window stays black. The base stub draws with the normal pipeline.
 */
public final class MiuiRenderNodeGuard {

    private static final String TAG = "MiuiRenderNodeGuard";
    private static boolean sInstalled;

    private MiuiRenderNodeGuard() {
    }

    public static void install() {
        if (sInstalled) {
            return;
        }
        try {
            Class<?> stub = Class.forName("android.graphics.RenderNodeStub");
            Field supplier = stub.getDeclaredField("sImplSupplier");
            supplier.setAccessible(true);
            Class<?> supplierType = Class.forName("com.miui.base.MiuiStubUtil$ImplSupplier");
            Constructor<?> constructor = stub.getDeclaredConstructor();
            constructor.setAccessible(true);
            Object fallback = Proxy.newProxyInstance(
                    MiuiRenderNodeGuard.class.getClassLoader(),
                    new Class<?>[]{supplierType},
                    (proxy, method, args) -> {
                        if ("newInstance".equals(method.getName()) && method.getParameterTypes().length == 0) {
                            return constructor.newInstance();
                        }
                        return null;
                    });
            putStatic(supplier, fallback);
            sInstalled = true;
            Slog.i(TAG, "RenderNodeStub uses the base implementation");
        } catch (ClassNotFoundException notMiui) {
            sInstalled = true;
        } catch (Throwable t) {
            Slog.w(TAG, "skip RenderNodeStub: " + t.getMessage());
        }
    }

    private static void putStatic(Field field, Object value) throws Exception {
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        Object unsafe = theUnsafe.get(null);
        Method base = unsafeClass.getDeclaredMethod("staticFieldBase", Field.class);
        Method offset = unsafeClass.getDeclaredMethod("staticFieldOffset", Field.class);
        Method put = unsafeClass.getDeclaredMethod("putObject", Object.class, long.class, Object.class);
        base.setAccessible(true);
        offset.setAccessible(true);
        put.setAccessible(true);
        Object fieldBase = base.invoke(unsafe, field);
        long fieldOffset = (Long) offset.invoke(unsafe, field);
        put.invoke(unsafe, fieldBase, fieldOffset, value);
    }
}
