package black.com.android.internal.policy;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("com.android.internal.policy.PhoneWindow$WindowManagerHolder")
public interface PhoneWindow {
    @BStaticField
    IInterface sWindowManager();
}
