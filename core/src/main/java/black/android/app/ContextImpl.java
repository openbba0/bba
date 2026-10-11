package black.android.app;

import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageManager;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BMethod;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.app.ContextImpl")
public interface ContextImpl {
    @BField
    String mBasePackageName();

    @BField
    ContentResolver mContentResolver();

    @BField
    Object mPackageInfo();

    @BField
    PackageManager mPackageManager();

    @BStaticMethod
    Object createAppContext();

    @BMethod
    Context getReceiverRestrictedContext();

    @BMethod
    void setOuterContext(Context Context0);

    @BMethod
    Object getAttributionSource();

    @BMethod
    PackageManager getPackageManager();
}
