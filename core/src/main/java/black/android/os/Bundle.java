package black.android.os;

import android.os.IBinder;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.os.Bundle")
public interface Bundle {
    @BMethod
    IBinder getIBinder(String String0);

    @BMethod
    void putIBinder(String String0, IBinder IBinder1);
}
