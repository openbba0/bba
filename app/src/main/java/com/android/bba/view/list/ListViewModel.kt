package com.android.bba.view.list

import androidx.lifecycle.MutableLiveData
import com.android.bba.bean.InstalledAppBean
import com.android.bba.data.AppsRepository
import com.android.bba.view.base.BaseViewModel


class ListViewModel(private val repo: AppsRepository) : BaseViewModel() {

    val appsLiveData = MutableLiveData<List<InstalledAppBean>>()

    val loadingLiveData = MutableLiveData<Boolean>()

    fun previewInstalledList() {
        repo.preloadInstallList()
    }

    fun getInstallAppList(userID: Int) {
        launchOnUI { repo.getInstalledAppList(userID, loadingLiveData, appsLiveData) }
    }
}
