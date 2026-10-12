package com.android.bba.data

import android.content.pm.ApplicationInfo
import android.net.Uri
import android.util.Log
import android.webkit.URLUtil
import androidx.lifecycle.MutableLiveData
import java.io.File
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import com.android.bb.BlackBoxCore
import com.android.bb.utils.AbiUtils
import com.android.bba.R
import com.android.bba.app.AppManager
import com.android.bba.bean.AppInfo
import com.android.bba.bean.InstalledAppBean
import com.android.bba.util.InstalledAppsPermission
import com.android.bba.util.MemoryManager
import com.android.bba.util.getString


class AppsRepository {
    val TAG: String = "AppsRepository"
    private var mInstalledList = mutableListOf<AppInfo>()

    // WelcomeActivity finishes immediately after starting the scan. A ViewModel
    // scope would be cancelled with that Activity, so the first install list stays empty.
    private val preloadScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val scanMonitor = Any()
    private var scanJob: Job? = null

    
    private fun safeLoadAppLabel(applicationInfo: ApplicationInfo): String {
        return try {
            BlackBoxCore.getPackageManager().getApplicationLabel(applicationInfo).toString()
        } catch (e: Exception) {
            Log.w(TAG, "Failed to load label for ${applicationInfo.packageName}: ${e.message}")
            applicationInfo.packageName 
        }
    }

    
    private fun safeLoadAppIcon(
            applicationInfo: ApplicationInfo
    ): android.graphics.drawable.Drawable? {
        return try {
            
            if (MemoryManager.shouldSkipIconLoading()) {
                Log.w(
                        TAG,
                        "Memory usage high (${MemoryManager.getMemoryUsagePercentage()}%), skipping icon for ${applicationInfo.packageName}"
                )
                return null
            }

            val icon = BlackBoxCore.getPackageManager().getApplicationIcon(applicationInfo)

            
            if (icon is android.graphics.drawable.BitmapDrawable) {
                val bitmap = icon.bitmap
                
                if (bitmap.width > 96 || bitmap.height > 96) {
                    try {
                        val scaledBitmap =
                                android.graphics.Bitmap.createScaledBitmap(bitmap, 96, 96, true)
                        android.graphics.drawable.BitmapDrawable(
                                BlackBoxCore.getPackageManager()
                                        .getResourcesForApplication(applicationInfo.packageName),
                                scaledBitmap
                        )
                    } catch (e: Exception) {
                        Log.w(
                                TAG,
                                "Failed to scale icon for ${applicationInfo.packageName}: ${e.message}"
                        )
                        icon
                    }
                } else {
                    icon
                }
            } else {
                icon
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to load icon for ${applicationInfo.packageName}: ${e.message}")
            null 
        }
    }

    fun preloadInstallList() {
        synchronized(scanMonitor) {
            if (scanJob?.isActive == true) return
            scanJob = preloadScope.launch { previewInstallList() }
        }
    }

    private fun ensureScanStarted(): Job? {
        synchronized(scanMonitor) {
            if (mInstalledList.isNotEmpty()) return null
            if (scanJob?.isActive != true) {
                scanJob = preloadScope.launch { previewInstallList() }
            }
            return scanJob
        }
    }

    fun previewInstallList() {
        try {
            val context = com.android.bb.BlackBoxCore.getContext()
            if (context != null && InstalledAppsPermission.needsRequest(context)) {
                return
            }
            synchronized(mInstalledList) {
                val installedApplications: List<ApplicationInfo> =
                        BlackBoxCore.getPackageManager().getInstalledApplications(0)
                val installedList = mutableListOf<AppInfo>()

                for (installedApplication in installedApplications) {
                    try {
                        val file = File(installedApplication.sourceDir)

                        if ((installedApplication.flags and ApplicationInfo.FLAG_SYSTEM) != 0)
                                continue

                        if (!AbiUtils.isSupport(file)) continue

                        
                        if (BlackBoxCore.get().isBlackBoxApp(installedApplication.packageName)) {
                            Log.d(
                                    TAG,
                                    "Filtering out BlackBox app: ${installedApplication.packageName}"
                            )
                            continue
                        }

                        val isXpModule = false

                        val info =
                                AppInfo(
                                        safeLoadAppLabel(installedApplication),
                                        safeLoadAppIcon(
                                                installedApplication
                                        ), 
                                        installedApplication.packageName,
                                        installedApplication.sourceDir,
                                        isXpModule
                                )
                        installedList.add(info)
                    } catch (e: Exception) {
                        Log.e(
                                TAG,
                                "Error processing app ${installedApplication.packageName}: ${e.message}"
                        )
                    }
                }
                this.mInstalledList.clear()
                this.mInstalledList.addAll(installedList)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in previewInstallList: ${e.message}")
        }
    }

    suspend fun getInstalledAppList(
            userID: Int,
            loadingLiveData: MutableLiveData<Boolean>,
            appsLiveData: MutableLiveData<List<InstalledAppBean>>
    ) {
        try {
            loadingLiveData.postValue(true)
            ensureScanStarted()?.join()
            synchronized(mInstalledList) {
                val blackBoxCore = BlackBoxCore.get()
                Log.d(TAG, mInstalledList.joinToString(","))
                val newInstalledList =
                        mInstalledList.map {
                            InstalledAppBean(
                                    it.name,
                                    it.icon, 
                                    it.packageName,
                                    it.sourceDir,
                                    blackBoxCore.isInstalled(it.packageName, userID)
                            )
                        }
                appsLiveData.postValue(newInstalledList)
                loadingLiveData.postValue(false)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in getInstalledAppList: ${e.message}")
            loadingLiveData.postValue(false)
            appsLiveData.postValue(emptyList())
        }
    }

    fun getVmInstallList(userId: Int, appsLiveData: MutableLiveData<List<AppInfo>>) {
        try {
            
            if (MemoryManager.isMemoryCritical()) {
                Log.w(
                        TAG,
                        "Memory critical (${MemoryManager.getMemoryUsagePercentage()}%), forcing garbage collection"
                )
                MemoryManager.forceGarbageCollectionIfNeeded()
            }

            val blackBoxCore = BlackBoxCore.get()

            
            val users = blackBoxCore.users
            Log.d(TAG, "getVmInstallList: userId=$userId, total users=${users.size}")
            users.forEach { user -> Log.d(TAG, "User: id=${user.id}, name=${user.name}") }

            val sortListData = AppManager.mRemarkSharedPreferences.getString("AppList$userId", "")
            val sortList = sortListData?.split(",")

            
            var applicationList: List<ApplicationInfo>? = null
            var retryCount = 0
            val maxRetries = 3

            while (applicationList == null && retryCount < maxRetries) {
                try {
                    applicationList = blackBoxCore.getInstalledApplications(0, userId)
                    if (applicationList == null) {
                        Log.w(
                                TAG,
                                "getVmInstallList: Attempt ${retryCount + 1} returned null, retrying..."
                        )
                        retryCount++
                        Thread.sleep(100) 
                    }
                } catch (e: Exception) {
                    Log.e(
                            TAG,
                            "getVmInstallList: Error getting applications on attempt ${retryCount + 1}: ${e.message}"
                    )
                    retryCount++
                    if (retryCount < maxRetries) {
                        Thread.sleep(200) 
                    }
                }
            }

            
            if (applicationList == null) {
                Log.e(
                        TAG,
                        "getVmInstallList: applicationList is null for userId=$userId after $maxRetries attempts"
                )
                appsLiveData.postValue(emptyList())
                return
            }

            
            Log.d(
                    TAG,
                    "getVmInstallList: userId=$userId, applicationList.size=${applicationList.size}"
            )
            if (applicationList.isNotEmpty()) {
                Log.d(TAG, "First app: ${applicationList.first().packageName}")
            } else {
                Log.w(TAG, "getVmInstallList: No applications found for userId=$userId")
            }

            val appInfoList = mutableListOf<AppInfo>()

            
            val sortedApplicationList =
                    if (!sortList.isNullOrEmpty()) {
                        try {
                            applicationList.sortedWith(AppsSortComparator(sortList))
                        } catch (e: Exception) {
                            Log.e(TAG, "getVmInstallList: Error sorting applications: ${e.message}")
                            applicationList 
                        }
                    } else {
                        applicationList
                    }

            
            sortedApplicationList.forEachIndexed { index, applicationInfo ->
                try {
                    
                    if (index > 0 && index % 25 == 0) {
                        if (MemoryManager.isMemoryCritical()) {
                            Log.w(TAG, "Memory critical during processing, forcing GC")
                            MemoryManager.forceGarbageCollectionIfNeeded()
                        }
                    }

                    
                    if (applicationInfo == null) {
                        Log.w(
                                TAG,
                                "getVmInstallList: Skipping null applicationInfo at index $index"
                        )
                        return@forEachIndexed
                    }

                    
                    if (applicationInfo.packageName.isNullOrBlank()) {
                        Log.w(
                                TAG,
                                "getVmInstallList: Skipping app with null/blank package name at index $index"
                        )
                        return@forEachIndexed
                    }

                    val info =
                            AppInfo(
                                    safeLoadAppLabel(applicationInfo),
                                    safeLoadAppIcon(
                                            applicationInfo
                                    ), 
                                    applicationInfo.packageName,
                                    applicationInfo.sourceDir ?: "",
                                    false
                            )

                    appInfoList.add(info)

                    
                    if (index > 0 && index % 50 == 0) {
                        Log.d(
                                TAG,
                                "getVmInstallList: Processed $index/${sortedApplicationList.size} apps - ${MemoryManager.getMemoryInfo()}"
                        )
                    }
                } catch (e: Exception) {
                    Log.e(
                            TAG,
                            "getVmInstallList: Error processing app at index $index (${applicationInfo?.packageName}): ${e.message}"
                    )
                    
                }
            }

            Log.d(
                    TAG,
                    "getVmInstallList: processed ${appInfoList.size} apps - ${MemoryManager.getMemoryInfo()}"
            )

            
            
            if (appInfoList.isEmpty()) {
                Log.d(
                        TAG,
                        "getVmInstallList: No virtual apps found for userId=$userId, showing empty list (correct for new users)"
                )
            } else {
                Log.d(
                        TAG,
                        "getVmInstallList: Showing ${appInfoList.size} virtual apps for userId=$userId"
                )
            }

            
            try {
                appsLiveData.postValue(appInfoList)
            } catch (e: Exception) {
                Log.e(TAG, "getVmInstallList: Error posting to LiveData: ${e.message}")
                
                try {
                    android.os.Handler(android.os.Looper.getMainLooper()).post {
                        try {
                            appsLiveData.postValue(appInfoList)
                        } catch (e2: Exception) {
                            Log.e(
                                    TAG,
                                    "getVmInstallList: Fallback posting also failed: ${e2.message}"
                            )
                        }
                    }
                } catch (e3: Exception) {
                    Log.e(
                            TAG,
                            "getVmInstallList: Could not schedule fallback posting: ${e3.message}"
                    )
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in getVmInstallList: ${e.message}")
            try {
                appsLiveData.postValue(emptyList())
            } catch (e2: Exception) {
                Log.e(TAG, "getVmInstallList: Error posting empty list: ${e2.message}")
            }
        }
    }

    fun getAllVmInstallList(appsLiveData: MutableLiveData<List<AppInfo>>) {
        try {
            val blackBoxCore = BlackBoxCore.get()
            val users = blackBoxCore.users.sortedBy { it.id }
            val merged = mutableListOf<AppInfo>()
            for (user in users) {
                val applicationList = try {
                    blackBoxCore.getInstalledApplications(0, user.id)
                } catch (e: Exception) {
                    Log.e(TAG, "getAllVmInstallList: user ${user.id}: ${e.message}")
                    null
                } ?: continue
                for (applicationInfo in applicationList) {
                    if (applicationInfo?.packageName.isNullOrBlank()) continue
                    merged.add(
                        AppInfo(
                            safeLoadAppLabel(applicationInfo),
                            safeLoadAppIcon(applicationInfo),
                            applicationInfo.packageName,
                            applicationInfo.sourceDir ?: "",
                            false,
                            user.id,
                        )
                    )
                }
            }
            val indexed = merged.groupBy { it.packageName }.flatMap { (_, items) ->
                items.sortedBy { it.userId }.mapIndexed { index, info -> info.copy(cloneIndex = index + 1) }
            }
            val order = AppManager.mRemarkSharedPreferences.getString("AppListAll", "")
                ?.split(",")
                ?.mapIndexed { index, key -> key to index }
                ?.toMap()
                .orEmpty()
            val sorted = indexed.sortedWith(
                compareBy(
                    { order["${it.packageName}#${it.userId}"] ?: Int.MAX_VALUE },
                    { it.name.lowercase() },
                    { it.cloneIndex },
                )
            )
            appsLiveData.postValue(sorted)
        } catch (e: Exception) {
            Log.e(TAG, "Error in getAllVmInstallList: ${e.message}")
            appsLiveData.postValue(emptyList())
        }
    }

    private fun resolvePackageName(source: String): String? {
        return try {
            if (URLUtil.isValidUrl(source)) {
                null
            } else if (File(source).exists()) {
                BlackBoxCore.getPackageManager().getPackageArchiveInfo(source, 0)?.packageName
            } else {
                source.takeIf { it.isNotBlank() && !it.contains("/") }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Could not resolve package name: ${e.message}")
            null
        }
    }

    private fun nextUserId(packageName: String): Int {
        val core = BlackBoxCore.get()
        val ids = core.users.map { it.id }.toSet()
        var candidate = 0
        while (candidate in ids && core.isInstalled(packageName, candidate)) {
            candidate++
        }
        return candidate
    }

    fun installApk(source: String, userId: Int, resultLiveData: MutableLiveData<String>) {
        try {
            
            if (source.contains("blackbox") ||
                            source.contains("niunaijun") ||
                            source.contains("vspace") ||
                            source.contains("virtual")
            ) {
                
                try {
                    val blackBoxCore = BlackBoxCore.get()
                    val hostPackageName = BlackBoxCore.getHostPkg()

                    
                    if (!URLUtil.isValidUrl(source)) {
                        val file = File(source)
                        if (file.exists()) {
                            val packageInfo =
                                    BlackBoxCore.getPackageManager()
                                            .getPackageArchiveInfo(source, 0)
                            if (packageInfo != null && packageInfo.packageName == hostPackageName) {
                                resultLiveData.postValue(
                                        "Cannot install BBA from within BBA. This would create infinite recursion and is not allowed for security reasons."
                                )
                                return
                            }
                        }
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "Could not verify if this is BlackBox app: ${e.message}")
                }
            }

            val blackBoxCore = BlackBoxCore.get()
            val targetUser = resolvePackageName(source)?.let { nextUserId(it) } ?: userId
            val installResult =
                    if (URLUtil.isValidUrl(source)) {
                        val uri = Uri.parse(source)
                        blackBoxCore.installPackageAsUser(uri, targetUser)
                    } else {
                        blackBoxCore.installPackageAsUser(source, targetUser)
                    }

            if (installResult.success) {
                updateAppSortList(targetUser, installResult.packageName, true)
                resultLiveData.postValue(getString(R.string.install_success))
            } else {
                resultLiveData.postValue(getString(R.string.install_fail, installResult.msg))
            }
            scanUser()
        } catch (e: Exception) {
            Log.e(TAG, "Error installing APK: ${e.message}")
            resultLiveData.postValue("Installation failed: ${e.message}")
        }
    }

    fun unInstall(packageName: String, userID: Int, resultLiveData: MutableLiveData<String>) {
        try {
            BlackBoxCore.get().uninstallPackageAsUser(packageName, userID)
            updateAppSortList(userID, packageName, false)
            scanUser()
            resultLiveData.postValue(getString(R.string.uninstall_success))
        } catch (e: Exception) {
            Log.e(TAG, "Error uninstalling APK: ${e.message}")
            resultLiveData.postValue("Uninstallation failed: ${e.message}")
        }
    }

    fun launchApk(packageName: String, userId: Int, launchLiveData: MutableLiveData<Boolean>) {
        try {
            val result = BlackBoxCore.get().launchApk(packageName, userId)
            launchLiveData.postValue(result)
        } catch (e: Exception) {
            Log.e(TAG, "Error launching APK: ${e.message}")
            launchLiveData.postValue(false)
        }
    }

    fun clearApkData(packageName: String, userID: Int, resultLiveData: MutableLiveData<String>) {
        try {
            BlackBoxCore.get().clearPackage(packageName, userID)
            resultLiveData.postValue(getString(R.string.clear_success))
        } catch (e: Exception) {
            Log.e(TAG, "Error clearing APK data: ${e.message}")
            resultLiveData.postValue("Clear failed: ${e.message}")
        }
    }

    
    private fun scanUser() {
        try {
            val blackBoxCore = BlackBoxCore.get()
            val userList = blackBoxCore.users

            if (userList.isEmpty()) {
                return
            }

            val id = userList.last().id

            if (blackBoxCore.getInstalledApplications(0, id).isEmpty()) {
                blackBoxCore.deleteUser(id)
                AppManager.mRemarkSharedPreferences.edit().apply {
                    remove("Remark$id")
                    remove("AppList$id")
                    apply()
                }
                scanUser()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in scanUser: ${e.message}")
        }
    }

    
    private fun updateAppSortList(userID: Int, pkg: String, isAdd: Boolean) {
        try {
            val savedSortList = AppManager.mRemarkSharedPreferences.getString("AppList$userID", "")

            val sortList = linkedSetOf<String>()
            if (savedSortList != null) {
                sortList.addAll(savedSortList.split(","))
            }

            if (isAdd) {
                sortList.add(pkg)
            } else {
                sortList.remove(pkg)
            }

            AppManager.mRemarkSharedPreferences.edit().apply {
                putString("AppList$userID", sortList.joinToString(","))
                apply()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error updating app sort list: ${e.message}")
        }
    }

    
    fun updateApkOrder(userID: Int, dataList: List<AppInfo>) {
        try {
            AppManager.mRemarkSharedPreferences.edit().apply {
                putString("AppList$userID", dataList.joinToString(",") { it.packageName })
                apply()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error updating APK order: ${e.message}")
        }
    }

    fun updateAllApkOrder(dataList: List<AppInfo>) {
        try {
            AppManager.mRemarkSharedPreferences.edit().apply {
                putString("AppListAll", dataList.joinToString(",") { "${it.packageName}#${it.userId}" })
                apply()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error updating all APK order: ${e.message}")
        }
    }
}
