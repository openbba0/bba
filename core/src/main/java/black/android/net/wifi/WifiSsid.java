package black.android.net.wifi;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.net.wifi.WifiSsid")
public interface WifiSsid {
    @BStaticMethod
    Object createFromAsciiEncoded(String asciiEncoded);
}
