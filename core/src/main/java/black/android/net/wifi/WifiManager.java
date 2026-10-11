package black.android.net.wifi;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("android.net.wifi.WifiManager")
public interface WifiManager {
    @BStaticField
    IInterface sService();

    @BField
    IInterface mService();
}
