package com.android.blackboxa.view.me

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.android.blackboxa.BuildConfig
import com.android.blackboxa.R
import com.android.blackboxa.databinding.FragmentMeBinding
import com.android.blackboxa.view.fake.FakeManagerActivity

class MeFragment : Fragment() {

    private var _binding: FragmentMeBinding? = null
    private val binding get() = _binding!!

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

        binding.fakeLocationRow.setOnClickListener {
            startActivity(Intent(requireContext(), FakeManagerActivity::class.java).putExtra("userID", 0))
        }
        binding.openSourceRow.setOnClickListener { openLink("https://github.com/ALEX5402/NewBlackbox") }
        binding.telegramRow.setOnClickListener { openLink("https://t.me/newblackboxa") }
    }

    private fun openLink(url: String) {
        startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
