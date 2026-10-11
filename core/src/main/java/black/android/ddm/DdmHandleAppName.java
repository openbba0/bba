package black.android.ddm;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.ddm.DdmHandleAppName")
public interface DdmHandleAppName {
    @BStaticMethod
    void setAppName(String String0, int i);
}
