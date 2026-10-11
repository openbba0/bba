package black.android.telephony;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.telephony.SmsManager")
public interface SmsManager {
    @BMethod
    Boolean getAutoPersisting();

    @BMethod
    void setAutoPersisting(boolean boolean0);
}
