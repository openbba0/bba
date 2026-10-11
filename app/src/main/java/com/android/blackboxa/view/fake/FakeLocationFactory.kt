package com.android.blackboxa.view.fake

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import com.android.blackboxa.data.FakeLocationRepository


class FakeLocationFactory(private val repo: FakeLocationRepository) :
    ViewModelProvider.NewInstanceFactory() {

    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        return FakeLocationViewModel(repo) as T
    }
}