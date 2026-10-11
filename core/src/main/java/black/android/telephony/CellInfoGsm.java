package black.android.telephony;

import android.telephony.CellIdentityGsm;
import android.telephony.CellSignalStrengthGsm;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BField;

@BClassName("android.telephony.CellInfoGsm")
public interface CellInfoGsm {
    @BConstructor
    CellInfoGsm _new();

    @BField
    CellIdentityGsm mCellIdentityGsm();

    @BField
    CellSignalStrengthGsm mCellSignalStrengthGsm();
}
