package black.java.lang;

import java.util.List;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("java.lang.ThreadGroup")
public interface ThreadGroup {
    @BField
    List<java.lang.ThreadGroup> groups();

    @BField
    java.lang.ThreadGroup parent();
}
