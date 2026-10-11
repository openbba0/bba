package black.android.content.res;

import android.content.pm.ApplicationInfo;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("android.content.res.CompatibilityInfo")
public interface CompatibilityInfo {
    @BConstructor
    CompatibilityInfo _new(ApplicationInfo ApplicationInfo0, int int1, int int2, boolean boolean3);

    @BConstructor
    CompatibilityInfo _new(ApplicationInfo ApplicationInfo0, int int1, int int2, boolean boolean3, int int4);

    @BStaticField
    Object DEFAULT_COMPATIBILITY_INFO();
}
