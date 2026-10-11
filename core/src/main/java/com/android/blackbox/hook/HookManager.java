package com.android.blackbox.hook;

import android.util.Log;

import java.util.HashMap;
import java.util.Map;

import com.android.blackbox.BlackBoxCore;
import com.android.blackbox.hook.app.AppInstrumentation;

import com.android.blackbox.hook.system.HCallbackProxy;
import com.android.blackbox.hook.system.IAccessibilityManagerProxy;
import com.android.blackbox.hook.device.IAuthServiceProxy;
import com.android.blackbox.hook.account.IAccountManagerProxy;
import com.android.blackbox.hook.system.IActivityClientProxy;
import com.android.blackbox.hook.system.IActivityManagerProxy;
import com.android.blackbox.hook.system.IActivityTaskManagerProxy;
import com.android.blackbox.hook.system.IAlarmManagerProxy;
import com.android.blackbox.hook.system.IAppOpsManagerProxy;
import com.android.blackbox.hook.system.IAppWidgetManagerProxy;
import com.android.blackbox.hook.content.IAttributionSourceProxy;
import com.android.blackbox.hook.device.IAutofillManagerProxy;
import com.android.blackbox.hook.device.ISensitiveContentProtectionManagerProxy;
import com.android.blackbox.hook.content.ISettingsSystemProxy;
import com.android.blackbox.hook.network.IConnectivityManagerProxy;
import com.android.blackbox.hook.device.ISystemSensorManagerProxy;
import com.android.blackbox.hook.content.IContentProviderProxy;
import com.android.blackbox.hook.miui.IXiaomiAttributionSourceProxy;
import com.android.blackbox.hook.miui.IXiaomiSettingsProxy;
import com.android.blackbox.hook.miui.IXiaomiMiuiServicesProxy;
import com.android.blackbox.hook.network.IDnsResolverProxy;
import com.android.blackbox.hook.device.IContextHubServiceProxy;
import com.android.blackbox.hook.device.IDeviceIdentifiersPolicyProxy;
import com.android.blackbox.hook.system.IDevicePolicyManagerProxy;
import com.android.blackbox.hook.system.IDisplayManagerProxy;
import com.android.blackbox.hook.device.IFingerprintManagerProxy;
import com.android.blackbox.hook.media.IGraphicsStatsProxy;
import com.android.blackbox.hook.system.IJobServiceProxy;
import com.android.blackbox.hook.system.ILauncherAppsProxy;
import com.android.blackbox.hook.location.ILocationManagerProxy;
import com.android.blackbox.hook.media.IMediaRouterServiceProxy;
import com.android.blackbox.hook.media.IMediaSessionManagerProxy;
import com.android.blackbox.hook.media.IAudioServiceProxy;
import com.android.blackbox.hook.device.ISensorPrivacyManagerProxy;
import com.android.blackbox.hook.system.ContentResolverProxy;
import com.android.blackbox.hook.miui.IMiuiSecurityManagerProxy;
import com.android.blackbox.hook.webview.SystemLibraryProxy;
import com.android.blackbox.hook.webview.ReLinkerProxy;
import com.android.blackbox.hook.media.MediaRecorderProxy;
import com.android.blackbox.hook.network.NetworkPermissionCompat;
import com.android.blackbox.hook.media.AudioRecordProxy;
import com.android.blackbox.hook.media.MediaRecorderClassProxy;
import com.android.blackbox.hook.storage.SQLiteDatabaseProxy;
import com.android.blackbox.hook.resources.ClassLoaderProxy;
import com.android.blackbox.hook.storage.FileSystemProxy;
import com.android.blackbox.hook.gms.GmsProxy;
import com.android.blackbox.hook.storage.LevelDbProxy;
import com.android.blackbox.hook.gms.DeviceIdProxy;
import com.android.blackbox.hook.account.GoogleAccountManagerProxy;
import com.android.blackbox.hook.account.AuthenticationProxy;
import com.android.blackbox.hook.gms.AndroidIdProxy;
import com.android.blackbox.hook.media.AudioPermissionProxy;
import com.android.blackbox.hook.network.NetworkPermissionCompat;

import com.android.blackbox.hook.network.INetworkManagementServiceProxy;
import com.android.blackbox.hook.system.INotificationManagerProxy;
import com.android.blackbox.hook.system.IPackageManagerProxy;
import com.android.blackbox.hook.system.IPermissionManagerProxy;
import com.android.blackbox.hook.storage.IPersistentDataBlockServiceProxy;
import com.android.blackbox.hook.telephony.IPhoneSubInfoProxy;
import com.android.blackbox.hook.device.IPowerManagerProxy;
import com.android.blackbox.hook.resources.ApkAssetsProxy;
import com.android.blackbox.hook.resources.ResourcesManagerProxy;
import com.android.blackbox.hook.system.IShortcutManagerProxy;
import com.android.blackbox.hook.storage.IStorageManagerProxy;
import com.android.blackbox.hook.storage.IStorageStatsManagerProxy;
import com.android.blackbox.hook.system.ISystemUpdateProxy;
import com.android.blackbox.hook.telephony.ISubProxy;
import com.android.blackbox.hook.telephony.ITelephonyProxy;
import com.android.blackbox.hook.telephony.ITelephonyRegistryProxy;
import com.android.blackbox.hook.system.IUserManagerProxy;
import com.android.blackbox.hook.device.IVibratorServiceProxy;
import com.android.blackbox.hook.network.IVpnManagerProxy;
import com.android.blackbox.hook.network.IWifiManagerProxy;
import com.android.blackbox.hook.network.IWifiScannerProxy;
import com.android.blackbox.hook.system.IWindowManagerProxy;
import com.android.blackbox.hook.system.ContentServiceStub;
import com.android.blackbox.hook.system.RestrictionsManagerStub;
import com.android.blackbox.hook.system.OsStub;
import com.android.blackbox.utils.Slog;
import com.android.blackbox.utils.compat.BuildCompat;
import com.android.blackbox.hook.content.ISettingsProviderProxy;
import com.android.blackbox.hook.resources.FeatureFlagUtilsProxy;
import com.android.blackbox.hook.resources.WorkManagerProxy;



public class HookManager {
    public static final String TAG = "HookManager";

    private static final HookManager sHookManager = new HookManager();

    private final Map<Class<?>, IInjectHook> mInjectors = new HashMap<>();

    public static HookManager get() {
        return sHookManager;
    }

    public void init() {
        if (BlackBoxCore.get().isBlackProcess() || BlackBoxCore.get().isServerProcess()) {
            addInjector(new IDisplayManagerProxy());
            addInjector(new OsStub());
            addInjector(new IActivityManagerProxy());
            addInjector(new IPackageManagerProxy());
            addInjector(new ITelephonyProxy());
            addInjector(new ISubProxy());
            addInjector(new HCallbackProxy());
            addInjector(new IAppOpsManagerProxy());
            addInjector(new INotificationManagerProxy());
            addInjector(new IAlarmManagerProxy());
            addInjector(new IAppWidgetManagerProxy());
            addInjector(new ContentServiceStub());
            addInjector(new IWindowManagerProxy());
            addInjector(new IUserManagerProxy());
            addInjector(new RestrictionsManagerStub());
            addInjector(new IMediaSessionManagerProxy());
            addInjector(new IAudioServiceProxy());
            addInjector(new ISensorPrivacyManagerProxy());
            addInjector(new ContentResolverProxy());
            addInjector(new SystemLibraryProxy());
            addInjector(new ReLinkerProxy());
            addInjector(new WorkManagerProxy());
            addInjector(new MediaRecorderProxy());
            addInjector(new AudioRecordProxy());
            addInjector(new IMiuiSecurityManagerProxy());
            addInjector(new ISettingsProviderProxy());
            addInjector(new FeatureFlagUtilsProxy());
            addInjector(new MediaRecorderClassProxy());
            addInjector(new SQLiteDatabaseProxy());
            addInjector(new ClassLoaderProxy());
            addInjector(new FileSystemProxy());
            addInjector(new GmsProxy());
            addInjector(new LevelDbProxy());
            addInjector(new DeviceIdProxy());
            addInjector(new GoogleAccountManagerProxy());
            addInjector(new AuthenticationProxy());
            addInjector(new AndroidIdProxy());
            addInjector(new AudioPermissionProxy());
            addInjector(new ILocationManagerProxy());
            addInjector(new IStorageManagerProxy());
            addInjector(new ILauncherAppsProxy());
            addInjector(new IJobServiceProxy());
            addInjector(new IAccessibilityManagerProxy());
            addInjector(new ITelephonyRegistryProxy());
            addInjector(new IDevicePolicyManagerProxy());
            addInjector(new IAccountManagerProxy());
            addInjector(new NetworkPermissionCompat());
            addInjector(new IConnectivityManagerProxy());
            addInjector(new IDnsResolverProxy());
                    addInjector(new IAttributionSourceProxy());
        addInjector(new IContentProviderProxy());
        addInjector(new ISettingsSystemProxy());
        addInjector(new ISystemSensorManagerProxy());
        
        
        addInjector(new IXiaomiAttributionSourceProxy());
        addInjector(new IXiaomiSettingsProxy());
        addInjector(new IXiaomiMiuiServicesProxy());
            addInjector(new IPhoneSubInfoProxy());
            addInjector(new IMediaRouterServiceProxy());
            addInjector(new IPowerManagerProxy());
            addInjector(new IContextHubServiceProxy());
            
            addInjector(new IVibratorServiceProxy());
            addInjector(new IPersistentDataBlockServiceProxy());
            addInjector(AppInstrumentation.get());
            
            addInjector(new IWifiManagerProxy());
            addInjector(new IWifiScannerProxy());
            addInjector(new ApkAssetsProxy());
            addInjector(new ResourcesManagerProxy());
            
            if (BuildCompat.isS()) {
                addInjector(new IActivityClientProxy(null));
                addInjector(new IVpnManagerProxy());
            }
            
            if (BuildCompat.isS()) {
                addInjector(new ISensitiveContentProtectionManagerProxy());
            }
            
            if (BuildCompat.isR()) {
                addInjector(new IPermissionManagerProxy());
            }
            
            if (BuildCompat.isQ()) {
                addInjector(new IActivityTaskManagerProxy());
                addInjector(new IAuthServiceProxy());
            }
            
            if (BuildCompat.isPie()) {
                addInjector(new ISystemUpdateProxy());
            }
            
            if (BuildCompat.isOreo()) {
                addInjector(new IAutofillManagerProxy());
                addInjector(new IDeviceIdentifiersPolicyProxy());
                addInjector(new IStorageStatsManagerProxy());
            }
            
            if (BuildCompat.isN_MR1()) {
                addInjector(new IShortcutManagerProxy());
            }
            
            if (BuildCompat.isN()) {
                addInjector(new INetworkManagementServiceProxy());
            }
            
            if (BuildCompat.isM()) {
                addInjector(new IFingerprintManagerProxy());
                addInjector(new IGraphicsStatsProxy());
            }
            
            if (BuildCompat.isL()) {
                addInjector(new IJobServiceProxy());
            }
        }
        injectAll();
    }

    public void checkEnv(Class<?> clazz) {
        IInjectHook iInjectHook = mInjectors.get(clazz);
        if (iInjectHook != null && iInjectHook.isBadEnv()) {
            Log.d(TAG, "checkEnv: " + clazz.getSimpleName() + " is bad env");
            iInjectHook.injectHook();
        }
    }

    public void checkAll() {
        for (Class<?> aClass : mInjectors.keySet()) {
            IInjectHook iInjectHook = mInjectors.get(aClass);
            if (iInjectHook != null && iInjectHook.isBadEnv()) {
                Log.d(TAG, "checkEnv: " + aClass.getSimpleName() + " is bad env");
                iInjectHook.injectHook();
            }
        }
    }

    void addInjector(IInjectHook injectHook) {
        mInjectors.put(injectHook.getClass(), injectHook);
    }

    void injectAll() {
        for (IInjectHook value : mInjectors.values()) {
            try {
                Slog.d(TAG, "hook: " + value);
                value.injectHook();
            } catch (Exception e) {
                Slog.d(TAG, "hook error: " + value);
                
                handleHookError(value, e);
            }
        }
    }

    
    private void handleHookError(IInjectHook hook, Exception e) {
        String hookName = hook.getClass().getSimpleName();
        
        
        Slog.e(TAG, "Hook failed: " + hookName + " - " + e.getMessage(), e);
        
        
        if (hookName.contains("ActivityManager") || 
            hookName.contains("PackageManager") ||
            hookName.contains("WebView") ||
            hookName.contains("ContentProvider")) {
            
            Slog.w(TAG, "Critical hook failed: " + hookName + ", attempting recovery");
            
            try {
                
                if (hook.isBadEnv()) {
                    Slog.d(TAG, "Attempting to recover hook: " + hookName);
                    hook.injectHook();
                }
            } catch (Exception recoveryException) {
                Slog.e(TAG, "Hook recovery failed: " + hookName, recoveryException);
            }
        }
    }

    
    public boolean areCriticalHooksInstalled() {
        String[] criticalHooks = {
            "IActivityManagerProxy",
            "IPackageManagerProxy",
            "IContentProviderProxy"
        };
        
        for (String hookName : criticalHooks) {
            boolean found = false;
            for (Class<?> hookClass : mInjectors.keySet()) {
                if (hookClass.getSimpleName().equals(hookName)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                Slog.w(TAG, "Critical hook missing: " + hookName);
                return false;
            }
        }
        
        Slog.d(TAG, "All critical hooks are installed");
        return true;
    }

    
    public void reinitializeHooks() {
        Slog.d(TAG, "Reinitializing all hooks");
        
        
        mInjectors.clear();
        
        
        init();
        
        Slog.d(TAG, "Hook reinitialization completed");
    }
}
