package black.android.widget;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("android.widget.Toast")
public interface Toast {
    @BStaticField
    IInterface sService();
}
