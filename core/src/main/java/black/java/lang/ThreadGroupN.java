package black.java.lang;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("java.lang.ThreadGroup")
public interface ThreadGroupN {
    @BField
    java.lang.ThreadGroup[] groups();

    @BField
    Integer ngroups();

    @BField
    java.lang.ThreadGroup parent();
}
