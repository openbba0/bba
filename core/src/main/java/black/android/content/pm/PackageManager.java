package black.android.content.pm;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BMethod;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.content.pm.PackageManager")
public interface PackageManager {
    @BStaticMethod
    void disableApplicationInfoCache();
}
