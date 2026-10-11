package black.android.os;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.os.Process")
public interface Process {
    @BStaticMethod
    void setArgV0(String String0);
}
