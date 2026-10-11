package black.android.telephony;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BField;

@BClassName("android.telephony.CellSignalStrengthGsm")
public interface CellSignalStrengthGsm {
    @BConstructor
    CellSignalStrengthGsm _new();

    @BField
    int mBitErrorRate();

    @BField
    int mSignalStrength();
}
