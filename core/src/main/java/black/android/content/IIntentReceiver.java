package black.android.content;

import android.content.Intent;
import android.os.Bundle;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.content.IIntentReceiver")
public interface IIntentReceiver {
    @BMethod
    void performReceive(Intent intent, int resultCode, String data, Bundle extras, boolean ordered, boolean sticky, int sendingUser);
}
