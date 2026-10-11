package black.java.io;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("java.io.File")
public interface File {
    @BStaticField
    Object fs();
}
