package com.android.bba.view.apps

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import com.android.bba.data.AppsRepository


@Suppress("UNCHECKED_CAST")
class AppsFactory(private val appsRepository: AppsRepository) : ViewModelProvider.NewInstanceFactory() {

    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        return AppsViewModel(appsRepository) as T
    }
}