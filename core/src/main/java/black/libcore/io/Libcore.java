package black.libcore.io;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("libcore.io.Libcore")
public interface Libcore {
    @BStaticField
    Object os();
}
