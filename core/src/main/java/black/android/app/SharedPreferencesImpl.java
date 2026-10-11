package black.android.app;

import java.io.File;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;

@BClassName("android.app.SharedPreferencesImpl")
public interface SharedPreferencesImpl {
    @BConstructor
    SharedPreferencesImpl _new(File File0, int int1);
}
