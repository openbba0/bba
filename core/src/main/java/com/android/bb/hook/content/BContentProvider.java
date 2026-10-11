package com.android.bb.hook.content;

import android.os.IInterface;


public interface BContentProvider {
    IInterface wrapper(final IInterface contentProviderProxy, final String appPkg, final String authority);
}
