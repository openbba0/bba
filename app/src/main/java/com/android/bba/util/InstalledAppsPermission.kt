package com.android.bba.util

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat

object InstalledAppsPermission {
    const val PERMISSION = "com.android.permission.GET_INSTALLED_APPS"

    fun needsRequest(context: Context): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false
        val defined = try {
            context.packageManager.getPermissionInfo(PERMISSION, 0)
            true
        } catch (e: PackageManager.NameNotFoundException) {
            false
        }
        if (!defined) return false
        return ContextCompat.checkSelfPermission(context, PERMISSION) !=
                PackageManager.PERMISSION_GRANTED
    }
}
