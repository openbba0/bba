package black.dalvik.system;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("dalvik.system.VMRuntime")
public interface VMRuntime {
    @BStaticMethod
    String getCurrentInstructionSet();

    @BStaticMethod
    Object getRuntime();

    @BStaticMethod
    Boolean is64BitAbi(String String0);

    @BMethod
    Boolean is64Bit();

    @BMethod
    Boolean isJavaDebuggable();

    @BMethod
    void setTargetSdkVersion(int int0);
}
