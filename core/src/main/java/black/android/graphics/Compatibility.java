package black.android.graphics;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;


@BClassName("android.graphics.Compatibility")
public interface Compatibility {
    @BStaticMethod
    void setTargetSdkVersion(int targetSdkVersion);
}
