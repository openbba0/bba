package black.android.content;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.content.IntentSender")
public interface IntentSender {
    @BField
    IInterface mTarget();
}
