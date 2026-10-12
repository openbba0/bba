package com.android.bb.core.system.spindle;

import android.content.Intent;
import android.os.Binder;
import android.os.Process;
import android.os.RemoteException;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import androidx.annotation.Keep;

import com.android.bb.BlackBoxCore;
import com.android.bb.R;
import com.android.bb.core.NativeCore;
import com.android.bb.core.system.ISystemService;
import com.android.bb.core.system.am.BActivityManagerService;
import com.android.bb.core.system.pm.BPackageManagerService;
import com.android.bb.utils.Slog;

public class BSpindleService extends ISpindleService.Stub implements ISystemService {
    public static final String TAG = "BSpindleService";
    /** Flip to true to bind the loopback port and accept agent registration. */
    public static final boolean ENABLED = true;
    private static final BSpindleService sService = new BSpindleService();
    private static final Map<Integer, Client> sClients = new HashMap<>();

    private static final class Client {
        final String name;
        final ISpindleClient callback;

        Client(String name, ISpindleClient callback) {
            this.name = name;
            this.callback = callback;
        }
    }

    public static BSpindleService get() {
        return sService;
    }

    @Override
    public void systemReady() {
        if (!ENABLED) {
            return;
        }
        NativeCore.spindleListen();
    }

    @Override
    public boolean register(String name, ISpindleClient client) {
        if (!ENABLED || client == null) {
            return false;
        }
        int callingUid = Binder.getCallingUid();
        if (callingUid != Process.myUid()) {
            Slog.w(TAG, "reject spindle register from uid " + callingUid);
            return false;
        }
        int pid = Binder.getCallingPid();
        String observed = readCmdline(pid);
        if (observed.isEmpty()) {
            if (name == null || name.isEmpty()) {
                Slog.w(TAG, "reject spindle register, cmdline unreadable pid=" + pid);
                return false;
            }
            Slog.w(TAG, "cmdline unreadable pid=" + pid + ", using " + name);
            observed = name;
        }
        String suffix = BlackBoxCore.getContext().getString(R.string.black_box_service_name);
        if (observed.endsWith(suffix)) {
            return false;
        }
        if (name != null && !name.isEmpty() && !name.equals(observed)) {
            Slog.w(TAG, "spindle register name " + name + " replaced by " + observed);
        }
        int id = NativeCore.spindleAdd(observed, pid, callingUid);
        if (id == 0) {
            return false;
        }
        synchronized (sClients) {
            sClients.put(id, new Client(observed, client));
        }
        try {
            client.asBinder().linkToDeath(() -> {
                forget(id);
                NativeCore.spindleDrop(id);
            }, 0);
        } catch (RemoteException e) {
            forget(id);
            NativeCore.spindleDrop(id);
            return false;
        }
        return true;
    }

    @Override
    public void emitLog(String text) {
        if (text == null || text.isEmpty()) {
            return;
        }
        NativeCore.spindleHostLog(Binder.getCallingPid(), text);
    }

    private static void forget(int id) {
        synchronized (sClients) {
            sClients.remove(id);
        }
    }

    @Keep
    public static byte[] deliver(int id, byte[] frame) {
        ISpindleClient client;
        synchronized (sClients) {
            Client held = sClients.get(id);
            client = held == null ? null : held.callback;
        }
        if (client == null) {
            return null;
        }
        long token = Binder.clearCallingIdentity();
        try {
            return client.onFrame(frame);
        } catch (RemoteException e) {
            Slog.w(TAG, "spindle deliver " + id + ": " + e.getMessage());
            return null;
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /** Stop and start the clone inside :black. If that fails, ask the main process. */
    @Keep
    public static boolean launch(String packageName) {
        if (packageName == null || packageName.isEmpty()) {
            return false;
        }
        try {
            BPackageManagerService.get().stopPackage(packageName, 0);
            Intent intent = BlackBoxCore.getBPackageManager().getLaunchIntentForPackage(packageName, 0);
            if (intent == null) {
                Slog.w(TAG, "no launch intent for " + packageName);
                return launchOnMain(packageName);
            }
            BActivityManagerService.get().startActivity(intent, 0);
            return true;
        } catch (Throwable t) {
            Slog.w(TAG, "spindle launch " + packageName + ": " + t.getMessage());
            return launchOnMain(packageName);
        }
    }

    /** Real activity start from the UI process, used when :black is background-restricted. */
    @Keep
    public static boolean launchOnMain(String packageName) {
        if (packageName == null || packageName.isEmpty()) {
            return false;
        }
        ISpindleClient main = null;
        String host = BlackBoxCore.getHostPkg();
        synchronized (sClients) {
            for (Client held : sClients.values()) {
                if (host.equals(held.name)) {
                    main = held.callback;
                    break;
                }
            }
        }
        if (main == null) {
            Slog.w(TAG, "spindle main client missing");
            return false;
        }
        long token = Binder.clearCallingIdentity();
        try {
            return main.launch(packageName);
        } catch (RemoteException e) {
            Slog.w(TAG, "spindle main launch " + packageName + ": " + e.getMessage());
            return false;
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    private static String readCmdline(int pid) {
        try (BufferedReader reader = new BufferedReader(new FileReader("/proc/" + pid + "/cmdline"))) {
            String line = reader.readLine();
            if (line == null) {
                return "";
            }
            int nul = line.indexOf('\0');
            if (nul >= 0) {
                line = line.substring(0, nul);
            }
            return line.trim();
        } catch (IOException e) {
            return "";
        }
    }
}
