package black.android.hardware.display;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.hardware.display.DisplayManagerGlobal")
public interface DisplayManagerGlobal {
    @BField
    IInterface mDm();

    @BStaticMethod
    Object getInstance();
}
