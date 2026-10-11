package black.android.os;

import android.os.Parcel;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.os.Bundle")
public interface BundleICS {
    @BField
    Parcel mParcelledData();
}
