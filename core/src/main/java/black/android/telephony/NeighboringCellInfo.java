package black.android.telephony;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.telephony.NeighboringCellInfo")
public interface NeighboringCellInfo {
    @BField
    int mCid();

    @BField
    int mLac();

    @BField
    int mRssi();
}
