package black.android.os;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.os.UserHandle")
public interface UserHandle {
    @BStaticMethod
    Integer myUserId();
}
