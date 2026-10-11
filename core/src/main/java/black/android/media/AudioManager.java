package black.android.media;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.media.AudioManager")
public interface AudioManager {
    @BStaticField
    IInterface sService();

    @BStaticMethod
    void getService();
}
