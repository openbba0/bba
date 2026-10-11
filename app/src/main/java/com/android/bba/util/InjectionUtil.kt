package com.android.bba.util

import com.android.bba.data.AppsRepository
import com.android.bba.data.FakeLocationRepository
import com.android.bba.data.GmsRepository

import com.android.bba.view.apps.AppsFactory
import com.android.bba.view.fake.FakeLocationFactory
import com.android.bba.view.gms.GmsFactory
import com.android.bba.view.list.ListFactory



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