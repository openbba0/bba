package com.android.bba.view.me

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.android.bba.BuildConfig
import com.android.bba.R
import com.android.bba.databinding.FragmentMeBinding
import com.android.bba.util.AppUpdate
import com.android.bba.view.fake.FakeManagerActivity
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

class MeFragment : Fragment() {

    private var _binding: FragmentMeBinding? = null
    private val binding get() = _binding!!

    private var busy = false
    private var checked = false
    private var latest: AppUpdate? = null
    private var pendingFile: File? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentMeBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val name = getString(R.string.app_name)
        val versionName = BuildConfig.VERSION_NAME
        binding.avatarLetter.text = name.firstOrNull()?.toString() ?: "B"
        binding.appNameText.text = name
        binding.currentVersionText.text = getString(R.string.me_current_version, versionName)
        binding.versionCodeText.text = getString(R.string.me_version_code, BuildConfig.VERSION_CODE)

        binding.updateRow.setOnClickListener { onUpdateClick() }
        binding.fakeLocationRow.setOnClickListener {
            startActivity(Intent(requireContext(), FakeManagerActivity::class.java).putExtra("userID", 0))
        }
        binding.openSourceRow.setOnClickListener { openLink("https://github.com/openbba0/bba") }
        binding.telegramRow.setOnClickListener { openLink("https://t.me/newblackboxa") }
    }

    override fun onResume() {
        super.onResume()
        val file = pendingFile
        if (file != null && AppUpdate.canInstall(requireContext())) {
            pendingFile = null
            AppUpdate.install(requireContext(), file)
            _binding?.updateStatus?.setText(R.string.me_update_installing)
        }
        if (!checked) {
            checked = true
            checkForUpdate()
        }
    }

    private fun onUpdateClick() {
        val update = latest
        if (update != null && update.versionCode > BuildConfig.VERSION_CODE) {
            download(update)
        } else if (!busy) {
            checkForUpdate()
        }
    }

    private fun checkForUpdate() {
        if (busy || _binding == null) return
        busy = true
        binding.updateStatus.setText(R.string.me_update_checking)
        viewLifecycleOwner.lifecycleScope.launch {
            val result = try {
                Result.success(withContext(Dispatchers.IO) { AppUpdate.fetchLatest() })
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Exception) {
                Result.failure(error)
            }
            if (_binding == null) return@launch
            busy = false
            result.onSuccess { update ->
                if (update.versionCode > BuildConfig.VERSION_CODE) {
                    latest = update
                    binding.updateStatus.text = getString(R.string.me_update_available, update.tag)
                } else {
                    latest = null
                    binding.updateStatus.setText(R.string.me_update_latest)
                }
            }.onFailure {
                latest = null
                binding.updateStatus.setText(R.string.me_update_failed)
            }
        }
    }

    private fun download(update: AppUpdate) {
        if (busy || _binding == null) return
        busy = true
        val dest = File(requireContext().cacheDir, "updates/${update.apkName}")
        viewLifecycleOwner.lifecycleScope.launch {
            val result = try {
                withContext(Dispatchers.IO) {
                    AppUpdate.download(update, dest) { pct ->
                        activity?.runOnUiThread {
                            val status = _binding?.updateStatus ?: return@runOnUiThread
                            if (pct < 0) {
                                status.setText(R.string.me_update_downloading_unknown)
                            } else {
                                status.text = getString(R.string.me_update_downloading, pct)
                            }
                        }
                    }
                }
                Result.success(dest)
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Exception) {
                Result.failure(error)
            }
            if (_binding == null) return@launch
            busy = false
            result.onSuccess { file -> promptInstall(file) }
                .onFailure { binding.updateStatus.setText(R.string.me_update_failed) }
        }
    }

    private fun promptInstall(file: File) {
        val context = requireContext()
        if (!AppUpdate.canInstall(context)) {
            pendingFile = file
            binding.updateStatus.setText(R.string.me_update_install_permission)
            AppUpdate.openInstallPermission(context)
            return
        }
        binding.updateStatus.setText(R.string.me_update_installing)
        AppUpdate.install(context, file)
    }

    private fun openLink(url: String) {
        startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
