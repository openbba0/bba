package black.android.os;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.os.DropBoxManager")
public interface DropBoxManager {
    @BField
    IInterface mService();
}
