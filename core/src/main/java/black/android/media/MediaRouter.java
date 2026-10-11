package black.android.media;

import android.os.IInterface;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BField;
import com.android.blackreflection.annotation.BStaticField;

@BClassName("android.media.MediaRouter")
public interface MediaRouter {
    @BStaticField
    Object sStatic();

    @BClassName("android.media.MediaRouter$Static")
    interface StaticKitkat {
        @BField
        IInterface mMediaRouterService();
    }

    @BClassName("android.media.MediaRouter$Static")
    interface Static {
        @BField
        IInterface mAudioService();
    }
}
