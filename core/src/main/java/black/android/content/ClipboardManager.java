package black.android.content;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.content.ClipboardManager")
public interface ClipboardManager {
    @BStaticField
    IInterface sService();

    @BStaticMethod
    IInterface getService();
}
