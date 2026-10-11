package com.android.blackboxa.view.list

import androidx.lifecycle.MutableLiveData
import com.android.blackboxa.bean.InstalledAppBean
import com.android.blackboxa.data.AppsRepository
import com.android.blackboxa.view.base.BaseViewModel


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
