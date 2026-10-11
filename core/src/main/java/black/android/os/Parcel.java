package black.android.os;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("android.os.Parcel")
public interface Parcel {
    @BStaticField
    int VAL_PARCELABLE();

    @BStaticField
    int VAL_PARCELABLEARRAY();
}
