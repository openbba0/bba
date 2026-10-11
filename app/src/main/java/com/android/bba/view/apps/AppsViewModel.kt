package com.android.bba.view.apps

import androidx.lifecycle.MutableLiveData
import com.android.bba.bean.AppInfo
import com.android.bba.data.AppsRepository
import com.android.bba.view.base.BaseViewModel
import android.util.Log


class AppsViewModel(private val repo: AppsRepository) : BaseViewModel() {

    val appsLiveData = MutableLiveData<List<AppInfo>>()

    val resultLiveData = MutableLiveData<String>()

    val launchLiveData = MutableLiveData<Boolean>()

    
    val updateSortLiveData = MutableLiveData<Boolean>()

    fun getInstalledApps(userId: Int) {
        launchOnUI {
            repo.getVmInstallList(userId, appsLiveData)
        }
    }

    fun getInstalledApps() {
        launchOnUI {
            repo.getAllVmInstallList(appsLiveData)
        }
    }
    
    
    fun getInstalledAppsWithRetry(userId: Int, maxRetries: Int = 3) {
        var retryCount = 0
        
        fun attemptLoad() {
            launchOnUI {
                repo.getAllVmInstallList(appsLiveData)
                
                
                val currentApps = appsLiveData.value
                if ((currentApps == null || currentApps.isEmpty()) && retryCount < maxRetries) {
                    retryCount++
                    Log.d("AppsViewModel", "No apps loaded, retrying... (${retryCount}/${maxRetries})")
                    
                    
                    android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                        attemptLoad()
                    }, 1000) 
                }
            }
        }
        
        attemptLoad()
    }

    fun install(source: String, userID: Int) {
        launchOnUI {
            repo.installApk(source, userID, resultLiveData)
        }
    }

    fun unInstall(packageName: String, userID: Int) {
        launchOnUI {
            repo.unInstall(packageName, userID, resultLiveData)
        }
    }

    fun clearApkData(packageName: String,userID: Int){
        launchOnUI {
            repo.clearApkData(packageName,userID,resultLiveData)
        }
    }

    fun launchApk(packageName: String, userID: Int) {
        launchOnUI {
            repo.launchApk(packageName, userID, launchLiveData)
        }
    }

    fun updateApkOrder(userID: Int,dataList:List<AppInfo>){
        launchOnUI {
            repo.updateApkOrder(userID,dataList)
        }
    }

    fun updateApkOrder(dataList: List<AppInfo>) {
        launchOnUI {
            repo.updateAllApkOrder(dataList)
        }
    }
}