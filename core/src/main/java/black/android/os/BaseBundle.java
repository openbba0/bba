package black.android.os;

import android.os.Parcel;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;

@BClassName("android.os.BaseBundle")
public interface BaseBundle {
    @BField
    Parcel mParcelledData();
}
