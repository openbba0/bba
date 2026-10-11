package black.libcore.io;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("libcore.io.ForwardingOs")
public interface ForwardingOs {
    @BField
    Object os();
}
