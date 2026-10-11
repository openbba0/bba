package com.android.blackboxa.view.setting

import android.content.Context
import android.content.Intent
import android.os.Bundle
import com.android.blackboxa.R
import com.android.blackboxa.databinding.ActivitySettingBinding
import com.android.blackboxa.util.inflate
import com.android.blackboxa.view.base.BaseActivity

class SettingActivity : BaseActivity() {

    private val viewBinding: ActivitySettingBinding by inflate()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(viewBinding.root)
        initToolbar(viewBinding.toolbarLayout.toolbar, R.string.setting, true)
        supportFragmentManager.beginTransaction()
                .replace(R.id.fragment, SettingFragment())
                .commit()
    }

    companion object{
        fun start(context: Context){
            val intent = Intent(context,SettingActivity::class.java)
            intent.action = Intent.ACTION_OPEN_DOCUMENT
            context.startActivity(intent)
        }
    }

}