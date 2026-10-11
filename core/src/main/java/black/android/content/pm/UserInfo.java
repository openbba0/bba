package black.android.content.pm;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("android.content.pm.UserInfo")
public interface UserInfo {
    @BConstructor
    Object _new(int id, String name, int flags);

    @BStaticField
    int FLAG_PRIMARY();
}
