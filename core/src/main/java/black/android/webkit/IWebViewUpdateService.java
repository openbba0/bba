package black.android.webkit;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.webkit.IWebViewUpdateService")
public interface IWebViewUpdateService {
    @BMethod
    String getCurrentWebViewPackageName();

    @BMethod
    Object waitForAndGetProvider();
}
