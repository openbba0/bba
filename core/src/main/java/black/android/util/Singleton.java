package black.android.util;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.util.Singleton")
public interface Singleton {
    @BField
    Object mInstance();

    @BMethod
    Object get();
}
