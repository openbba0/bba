package black.android.webkit;


import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BStaticField;
import com.android.blackreflection.annotation.BStaticMethod;

@BClassName("android.webkit.WebViewFactory")
public interface WebViewFactory {
    @BStaticField
    Boolean sWebViewSupported();

    @BStaticMethod
    Object getUpdateService();
}
