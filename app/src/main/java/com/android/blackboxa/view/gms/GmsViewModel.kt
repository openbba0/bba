package com.android.blackboxa.view.gms

import androidx.lifecycle.MutableLiveData
import com.android.blackboxa.bean.GmsBean
import com.android.blackboxa.bean.GmsInstallBean
import com.android.blackboxa.data.GmsRepository
import com.android.blackboxa.view.base.BaseViewModel


class GmsViewModel(private val mRepo: GmsRepository) : BaseViewModel() {

    val mInstalledLiveData = MutableLiveData<List<GmsBean>>()

    val mUpdateInstalledLiveData = MutableLiveData<GmsInstallBean>()

    fun getInstalledUser() {
        launchOnUI {
            mRepo.getGmsInstalledList(mInstalledLiveData)
        }
    }

    fun installGms(userID: Int) {
        launchOnUI {
            mRepo.installGms(userID,mUpdateInstalledLiveData)
        }
    }

    fun uninstallGms(userID: Int) {
        launchOnUI {
            mRepo.uninstallGms(userID,mUpdateInstalledLiveData)
        }
    }
}