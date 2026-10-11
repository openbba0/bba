package com.android.bb.hook;

import android.util.Log;

import java.util.HashMap;
import java.util.Map;

import com.android.bb.BlackBoxCore;
import com.android.bb.hook.app.AppInstrumentation;

import com.android.bb.hook.system.HCallbackProxy;
import com.android.bb.hook.system.IAccessibilityManagerProxy;
import com.android.bb.hook.device.IAuthServiceProxy;
import com.android.bb.hook.account.IAccountManagerProxy;
import com.android.bb.hook.system.IActivityClientProxy;
import com.android.bb.hook.system.IActivityManagerProxy;
import com.android.bb.hook.system.IActivityTaskManagerProxy;
import com.android.bb.hook.system.IAlarmManagerProxy;
import com.android.bb.hook.system.IAppOpsManagerProxy;
import com.android.bb.hook.system.IAppWidgetManagerProxy;
import com.android.bb.hook.content.IAttributionSourceProxy;
import com.android.bb.hook.device.IAutofillManagerProxy;
import com.android.bb.hook.device.ISensitiveContentProtectionManagerProxy;
import com.android.bb.hook.content.ISettingsSystemProxy;
import com.android.bb.hook.network.IConnectivityManagerProxy;
import com.android.bb.hook.device.ISystemSensorManagerProxy;
import com.android.bb.hook.content.IContentProviderProxy;
import com.android.bb.hook.miui.IXiaomiAttributionSourceProxy;
import com.android.bb.hook.miui.IXiaomiSettingsProxy;
import com.android.bb.hook.miui.IXiaomiMiuiServicesProxy;
import com.android.bb.hook.network.IDnsResolverProxy;
import com.android.bb.hook.device.IContextHubServiceProxy;
import com.android.bb.hook.device.IDeviceIdentifiersPolicyProxy;
import com.android.bb.hook.system.IDevicePolicyManagerProxy;
import com.android.bb.hook.system.IDisplayManagerProxy;
import com.android.bb.hook.device.IFingerprintManagerProxy;
import com.android.bb.hook.media.IGraphicsStatsProxy;
import com.android.bb.hook.system.IJobServiceProxy;
import com.android.bb.hook.system.ILauncherAppsProxy;
import com.android.bb.hook.location.ILocationManagerProxy;
import com.android.bb.hook.media.IMediaRouterServiceProxy;
import com.android.bb.hook.media.IMediaSessionManagerProxy;
import com.android.bb.hook.media.IAudioServiceProxy;
import com.android.bb.hook.device.ISensorPrivacyManagerProxy;
import com.android.bb.hook.system.ContentResolverProxy;
import com.android.bb.hook.miui.IMiuiSecurityManagerProxy;
import com.android.bb.hook.webview.SystemLibraryProxy;
import com.android.bb.hook.webview.ReLinkerProxy;
import com.android.bb.hook.media.MediaRecorderProxy;
import com.android.bb.hook.network.NetworkPermissionCompat;
import com.android.bb.hook.media.AudioRecordProxy;
import com.android.bb.hook.media.MediaRecorderClassProxy;
import com.android.bb.hook.storage.SQLiteDatabaseProxy;
import com.android.bb.hook.resources.ClassLoaderProxy;
import com.android.bb.hook.storage.FileSystemProxy;
import com.android.bb.hook.gms.GmsProxy;
import com.android.bb.hook.storage.LevelDbProxy;
import com.android.bb.hook.gms.DeviceIdProxy;
import com.android.bb.hook.account.GoogleAccountManagerProxy;
import com.android.bb.hook.account.AuthenticationProxy;
import com.android.bb.hook.gms.AndroidIdProxy;
import com.android.bb.hook.media.AudioPermissionProxy;
import com.android.bb.hook.network.NetworkPermissionCompat;

import com.android.bb.hook.network.INetworkManagementServiceProxy;
import com.android.bb.hook.system.INotificationManagerProxy;
import com.android.bb.hook.system.IPackageManagerProxy;
import com.android.bb.hook.system.IPermissionManagerProxy;
import com.android.bb.hook.storage.IPersistentDataBlockServiceProxy;
import com.android.bb.hook.telephony.IPhoneSubInfoProxy;
import com.android.bb.hook.device.IPowerManagerProxy;
import com.android.bb.hook.resources.ApkAssetsProxy;
import com.android.bb.hook.resources.ResourcesManagerProxy;
import com.android.bb.hook.system.IShortcutManagerProxy;
import com.android.bb.hook.storage.IStorageManagerProxy;
import com.android.bb.hook.storage.IStorageStatsManagerProxy;
import com.android.bb.hook.system.ISystemUpdateProxy;
import com.android.bb.hook.telephony.ISubProxy;
import com.android.bb.hook.telephony.ITelephonyProxy;
import com.android.bb.hook.telephony.ITelephonyRegistryProxy;
import com.android.bb.hook.system.IUserManagerProxy;
import com.android.bb.hook.device.IVibratorServiceProxy;
import com.android.bb.hook.network.IVpnManagerProxy;
import com.android.bb.hook.network.IWifiManagerProxy;
import com.android.bb.hook.network.IWifiScannerProxy;
import com.android.bb.hook.system.IWindowManagerProxy;
import com.android.bb.hook.system.ContentServiceStub;
import com.android.bb.hook.system.RestrictionsManagerStub;
import com.android.bb.hook.system.OsStub;
import com.android.bb.utils.Slog;
import com.android.bb.utils.compat.BuildCompat;
import com.android.bb.hook.content.ISettingsProviderProxy;
import com.android.bb.hook.resources.FeatureFlagUtilsProxy;
import com.android.bb.hook.resources.WorkManagerProxy;



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
