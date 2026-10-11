package black.android.net.wifi;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("android.net.wifi.WifiScanner")
public interface WifiScanner {
    @BStaticField
    String GET_AVAILABLE_CHANNELS_EXTRA();
}
