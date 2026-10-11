package black.com.android.internal.view.inputmethod;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.view.inputmethod.InputMethodManager")
public interface InputMethodManager {
    @BField
    IInterface mService();
}
