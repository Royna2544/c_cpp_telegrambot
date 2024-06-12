package com.royna.tgbotclient.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.royna.tgbotclient.R
import com.royna.tgbotclient.SocketCommandNative.getCurrentDestinationInfo
import com.royna.tgbotclient.databinding.FragmentCurrentSettingChildBinding

class CurrentSettingFragment : Fragment() {
    private var _binding: FragmentCurrentSettingChildBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentCurrentSettingChildBinding.inflate(inflater, container, false)
        update()
        return binding.root
    }

    fun update () {
        val info = getCurrentDestinationInfo()
        binding.currentIp.text = resources.getString(R.string.ip_address_fmt,
            info.ipaddr)
        binding.currentPort.text = resources.getString(R.string.port_fmt,
            info.port)
        binding.currentType.text = resources.getString(R.string.address_type_fmt,
            info.type.name)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onResume() {
        super.onResume()
        update()
    }
}