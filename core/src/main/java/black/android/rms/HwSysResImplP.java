package black.android.rms;

import java.util.Map;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.rms.HwSysResImpl")
public interface HwSysResImplP {
    @BField
    Map<Integer, java.util.ArrayList<String>> mWhiteListMap();
}
