package black.android.location;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.location.LocationRequest")
public interface LocationRequestL {
    @BField
    boolean mHideFromAppOps();

    @BField
    String mProvider();

    @BField
    Object mWorkSource();

    @BMethod
    String getProvider();
}
