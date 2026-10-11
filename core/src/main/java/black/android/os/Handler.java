package black.android.os;

import android.os.Handler.Callback;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.os.Handler")
public interface Handler {
    @BField
    Callback mCallback();
}
