package com.android.bba.view.setting

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreferenceCompat
import com.android.bb.BlackBoxCore
import com.android.bba.R
import com.android.bba.app.AppManager
import com.android.bba.util.toast
import com.android.bba.view.gms.GmsManagerActivity

class SettingFragment : PreferenceFragmentCompat() {

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        val preferenceView = super.onCreateView(inflater, container, savedInstanceState)
        val root = inflater.inflate(R.layout.fragment_setting, container, false)
        root.findViewById<ViewGroup>(R.id.prefHost).addView(
            preferenceView,
            ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT),
        )
        return root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        listView.setBackgroundColor(ContextCompat.getColor(requireContext(), R.color.surface))
        listView.clipToPadding = false
        listView.setPadding(0, 4, 0, 24)
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.setting, rootKey)

        initGms()

        invalidHideState {
            val rootHidePreference: Preference = (findPreference("root_hide")!!)
            val hideRoot = AppManager.mBlackBoxLoader.hideRoot()
            rootHidePreference.setDefaultValue(hideRoot)
            rootHidePreference
        }

        invalidHideState {
            val daemonPreference: Preference = (findPreference("daemon_enable")!!)
            val mDaemonEnable = AppManager.mBlackBoxLoader.daemonEnable()
            daemonPreference.setDefaultValue(mDaemonEnable)
            daemonPreference
        }

        invalidHideState {
            val disableFlagSecurePreference: Preference = (findPreference("disable_flag_secure")!!)
            val mDisableFlagSecure = AppManager.mBlackBoxLoader.disableFlagSecure()
            disableFlagSecurePreference.setDefaultValue(mDisableFlagSecure)
            disableFlagSecurePreference
        }

        invalidHideState {
            val dumpPreference: Preference = (findPreference("xhs_fingerprint_dump")!!)
            val enabled = AppManager.mBlackBoxLoader.xhsFingerprintDump()
            dumpPreference.setDefaultValue(enabled)
            if (dumpPreference is SwitchPreferenceCompat) {
                dumpPreference.isChecked = enabled
            }
            dumpPreference
        }

        initSendLogs()
    }

    private fun initGms() {
        val gmsManagerPreference: Preference = (findPreference("gms_manager")!!)

        if (BlackBoxCore.get().isSupportGms) {

            gmsManagerPreference.setOnPreferenceClickListener {
                GmsManagerActivity.start(requireContext())
                true
            }
        } else {
            gmsManagerPreference.summary = getString(R.string.no_gms)
            gmsManagerPreference.isEnabled = false
        }
    }

    private fun invalidHideState(block: () -> Preference) {
        val pref = block()
        pref.setOnPreferenceChangeListener { preference, newValue ->
            val tmpHide = (newValue == true)
            when (preference.key) {
                "root_hide" -> {

                    AppManager.mBlackBoxLoader.invalidHideRoot(tmpHide)
                }
                "daemon_enable" -> {
                    AppManager.mBlackBoxLoader.invalidDaemonEnable(tmpHide)
                }
                "disable_flag_secure" -> {
                    AppManager.mBlackBoxLoader.invalidDisableFlagSecure(tmpHide)
                }
                "xhs_fingerprint_dump" -> {
                    AppManager.mBlackBoxLoader.invalidXhsFingerprintDump(tmpHide)
                }
            }

            toast(R.string.restart_module)
            return@setOnPreferenceChangeListener true
        }
    }
    private fun initSendLogs() {
        val sendLogsPreference: Preference? = findPreference("send_logs")
        sendLogsPreference?.setOnPreferenceClickListener {
            it.isEnabled = false
            BlackBoxCore.get()
                    .sendLogs(
                            "Manual Log Upload from Settings",
                            true,
                            object : BlackBoxCore.LogSendListener {
                                override fun onSuccess() {
                                    activity?.runOnUiThread { sendLogsPreference.isEnabled = true }
                                }

                                override fun onFailure(error: String?) {
                                    activity?.runOnUiThread { sendLogsPreference.isEnabled = true }
                                }
                            }
                    )
            toast("Sending logs... (Check notifications for status)")
            true
        }
    }
}
