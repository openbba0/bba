package com.android.blackboxa.view.main

import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider
import com.android.blackboxa.util.InjectionUtil
import com.android.blackboxa.util.InstalledAppsPermission
import com.android.blackboxa.view.list.ListViewModel

class WelcomeActivity : AppCompatActivity() {

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        jump()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (!InstalledAppsPermission.needsRequest(this)) {
            previewInstalledAppList()
        }
        jump()
    }

    private fun jump() {
        MainActivity.start(this)
        finish()
    }

    private fun previewInstalledAppList(){
        val viewModel = ViewModelProvider(this,InjectionUtil.getListFactory()).get(ListViewModel::class.java)
        viewModel.previewInstalledList()
    }
}