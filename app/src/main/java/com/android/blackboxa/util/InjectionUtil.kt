package com.android.blackboxa.util

import com.android.blackboxa.data.AppsRepository
import com.android.blackboxa.data.FakeLocationRepository
import com.android.blackboxa.data.GmsRepository

import com.android.blackboxa.view.apps.AppsFactory
import com.android.blackboxa.view.fake.FakeLocationFactory
import com.android.blackboxa.view.gms.GmsFactory
import com.android.blackboxa.view.list.ListFactory



object InjectionUtil {

    private val appsRepository = AppsRepository()



    private val gmsRepository = GmsRepository()

    private val fakeLocationRepository = FakeLocationRepository()

    fun getAppsFactory() : AppsFactory {
        return AppsFactory(appsRepository)
    }

    fun getListFactory(): ListFactory {
        return ListFactory(appsRepository)
    }


    fun getGmsFactory():GmsFactory{
        return GmsFactory(gmsRepository)
    }

    fun getFakeLocationFactory():FakeLocationFactory{
        return FakeLocationFactory(fakeLocationRepository)
    }
}