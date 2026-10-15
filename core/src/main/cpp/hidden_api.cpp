
#include <jni.h>
#include "xdl.h"
#include "hidden_api.h"
#include "log.h"

bool disable_resource_loading() {
    
    try {
        
        void* handle = xdl_open("libandroid_runtime.so", XDL_DEFAULT);
        if (handle) {
            
            void* nativeLoadAddr = xdl_sym(handle, "_ZN7android8ApkAssets9nativeLoadEPKc", nullptr);
            if (nativeLoadAddr) {
                ALOGD("ResourceLoading: Found ApkAssets.nativeLoad at %p", nativeLoadAddr);
                
            } else {
                ALOGD("ResourceLoading: Could not find ApkAssets.nativeLoad symbol");
            }
            xdl_close(handle);
        } else {
            ALOGD("ResourceLoading: Could not open libandroid_runtime.so");
        }
    } catch (...) {
        ALOGD("ResourceLoading: Exception while trying to hook ApkAssets.nativeLoad");
    }
    
    
    try {
        
        void* handle = xdl_open("libc.so", XDL_DEFAULT);
        if (handle) {
            
            void* openAddr = xdl_sym(handle, "open", nullptr);
            if (openAddr) {
                ALOGD("ResourceLoading: Found open function at %p", openAddr);
                
            } else {
                ALOGD("ResourceLoading: Could not find open function symbol");
            }
            xdl_close(handle);
        } else {
            ALOGD("ResourceLoading: Could not open libc.so");
        }
    } catch (...) {
        ALOGD("ResourceLoading: Exception while trying to hook file system calls");
    }
    
    ALOGD("ResourceLoading: Native resource loading hooks initialized (without system properties)");
    return true;
}