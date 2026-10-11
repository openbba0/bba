package black.android.content.pm;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.content.pm.ApplicationInfo")
public interface ApplicationInfoP {
    @BField
    String[] splitClassLoaderNames();

    @BMethod
    void setHiddenApiEnforcementPolicy(int int0);
}
