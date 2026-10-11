package black.android.content;

import android.accounts.Account;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BConstructor;

@BClassName("android.content.SyncInfo")
public interface SyncInfo {
    @BConstructor
    SyncInfo _new(int int0, Account Account1, String String2, long long3);
}
