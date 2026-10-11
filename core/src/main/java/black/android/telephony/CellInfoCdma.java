package black.android.telephony;

import android.telephony.CellIdentityCdma;
import android.telephony.CellSignalStrengthCdma;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;
import com.android.blackreflection.annotation.BField;

@BClassName("android.telephony.CellInfoCdma")
public interface CellInfoCdma {
    @BConstructor
    CellInfoCdma _new();

    @BField
    CellIdentityCdma mCellIdentityCdma();

    @BField
    CellSignalStrengthCdma mCellSignalStrengthCdma();
}
