package com.android.blackbox.fake.hook;

import android.util.Log;

import java.util.HashMap;
import java.util.Map;

import com.android.blackbox.BlackBoxCore;
import com.android.blackbox.fake.delegate.AppInstrumentation;

import com.android.blackbox.fake.service.HCallbackProxy;
import com.android.blackbox.fake.service.IAccessibilityManagerProxy;
import com.android.blackbox.fake.service.IAuthServiceProxy;
import com.android.blackbox.fake.service.IAccountManagerProxy;
import com.android.blackbox.fake.service.IActivityClientProxy;
import com.android.blackbox.fake.service.IActivityManagerProxy;
import com.android.blackbox.fake.service.IActivityTaskManagerProxy;
import com.android.blackbox.fake.service.IAlarmManagerProxy;
import com.android.blackbox.fake.service.IAppOpsManagerProxy;
import com.android.blackbox.fake.service.IAppWidgetManagerProxy;
import com.android.blackbox.fake.service.IAttributionSourceProxy;
import com.android.blackbox.fake.service.IAutofillManagerProxy;
import com.android.blackbox.fake.service.ISensitiveContentProtectionManagerProxy;
import com.android.blackbox.fake.service.ISettingsSystemProxy;
import com.android.blackbox.fake.service.IConnectivityManagerProxy;
import com.android.blackbox.fake.service.ISystemSensorManagerProxy;
import com.android.blackbox.fake.service.IContentProviderProxy;
import com.android.blackbox.fake.service.IXiaomiAttributionSourceProxy;
import com.android.blackbox.fake.service.IXiaomiSettingsProxy;
import com.android.blackbox.fake.service.IXiaomiMiuiServicesProxy;
import com.android.blackbox.fake.service.IDnsResolverProxy;
import com.android.blackbox.fake.service.IContextHubServiceProxy;
import com.android.blackbox.fake.service.IDeviceIdentifiersPolicyProxy;
import com.android.blackbox.fake.service.IDevicePolicyManagerProxy;
import com.android.blackbox.fake.service.IDisplayManagerProxy;
import com.android.blackbox.fake.service.IFingerprintManagerProxy;
import com.android.blackbox.fake.service.IGraphicsStatsProxy;
import com.android.blackbox.fake.service.IJobServiceProxy;
import com.android.blackbox.fake.service.ILauncherAppsProxy;
import com.android.blackbox.fake.service.ILocationManagerProxy;
import com.android.blackbox.fake.service.IMediaRouterServiceProxy;
import com.android.blackbox.fake.service.IMediaSessionManagerProxy;
import com.android.blackbox.fake.service.IAudioServiceProxy;
import com.android.blackbox.fake.service.ISensorPrivacyManagerProxy;
import com.android.blackbox.fake.service.ContentResolverProxy;
import com.android.blackbox.fake.service.IMiuiSecurityManagerProxy;
import com.android.blackbox.fake.service.SystemLibraryProxy;
import com.android.blackbox.fake.service.ReLinkerProxy;
import com.android.blackbox.fake.service.MediaRecorderProxy;
import com.android.blackbox.fake.service.NetworkPermissionCompat;
import com.android.blackbox.fake.service.AudioRecordProxy;
import com.android.blackbox.fake.service.MediaRecorderClassProxy;
import com.android.blackbox.fake.service.SQLiteDatabaseProxy;
import com.android.blackbox.fake.service.ClassLoaderProxy;
import com.android.blackbox.fake.service.FileSystemProxy;
import com.android.blackbox.fake.service.GmsProxy;
import com.android.blackbox.fake.service.LevelDbProxy;
import com.android.blackbox.fake.service.DeviceIdProxy;
import com.android.blackbox.fake.service.GoogleAccountManagerProxy;
import com.android.blackbox.fake.service.AuthenticationProxy;
import com.android.blackbox.fake.service.AndroidIdProxy;
import com.android.blackbox.fake.service.AudioPermissionProxy;
import com.android.blackbox.fake.service.NetworkPermissionCompat;

import com.android.blackbox.fake.service.INetworkManagementServiceProxy;
import com.android.blackbox.fake.service.INotificationManagerProxy;
import com.android.blackbox.fake.service.IPackageManagerProxy;
import com.android.blackbox.fake.service.IPermissionManagerProxy;
import com.android.blackbox.fake.service.IPersistentDataBlockServiceProxy;
import com.android.blackbox.fake.service.IPhoneSubInfoProxy;
import com.android.blackbox.fake.service.IPowerManagerProxy;
import com.android.blackbox.fake.service.ApkAssetsProxy;
import com.android.blackbox.fake.service.ResourcesManagerProxy;
import com.android.blackbox.fake.service.IShortcutManagerProxy;
import com.android.blackbox.fake.service.IStorageManagerProxy;
import com.android.blackbox.fake.service.IStorageStatsManagerProxy;
import com.android.blackbox.fake.service.ISystemUpdateProxy;
import com.android.blackbox.fake.service.ITelephonyManagerProxy;
import com.android.blackbox.fake.service.ITelephonyRegistryProxy;
import com.android.blackbox.fake.service.IUserManagerProxy;
import com.android.blackbox.fake.service.IVibratorServiceProxy;
import com.android.blackbox.fake.service.IVpnManagerProxy;
import com.android.blackbox.fake.service.IWifiManagerProxy;
import com.android.blackbox.fake.service.IWifiScannerProxy;
import com.android.blackbox.fake.service.IWindowManagerProxy;
import com.android.blackbox.fake.service.context.ContentServiceStub;
import com.android.blackbox.fake.service.context.RestrictionsManagerStub;
import com.android.blackbox.fake.service.libcore.OsStub;
import com.android.blackbox.utils.Slog;
import com.android.blackbox.utils.compat.BuildCompat;
import com.android.blackbox.fake.service.ISettingsProviderProxy;
import com.android.blackbox.fake.service.FeatureFlagUtilsProxy;
import com.android.blackbox.fake.service.WorkManagerProxy;



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
            addInjector(new ITelephonyManagerProxy());
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
