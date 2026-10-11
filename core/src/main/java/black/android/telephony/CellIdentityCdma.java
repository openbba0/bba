package black.android.telephony;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BField;

@BClassName("android.telephony.CellIdentityCdma")
public interface CellIdentityCdma {
    @BConstructor
    CellIdentityCdma _new();

    @BField
    int mBasestationId();

    @BField
    int mNetworkId();

    @BField
    int mSystemId();
}
