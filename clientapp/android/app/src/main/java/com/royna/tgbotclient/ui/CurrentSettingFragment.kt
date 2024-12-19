package com.royna.tgbotclient.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.royna.tgbotclient.R
import com.royna.tgbotclient.databinding.FragmentCurrentSettingChildBinding
import com.royna.tgbotclient.net.SocketContext
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

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

   private fun update() = viewLifecycleOwner.lifecycleScope.launch {
        withContext(Dispatchers.IO) {
            val info = SocketContext.getInstance().destination
            val hostname = info.hostname
            val port = info.port

             withContext(Dispatchers.Main) {
                 if (_binding != null) {
                     synchronized(_binding as Any) {
                         binding.currentIp.text = resources.getString(
                             R.string.ip_address_fmt, hostname
                         )
                         binding.currentPort.text = resources.getString(
                             R.string.port_fmt, port
                         )
                     }
                 }
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        synchronized(_binding as Any) {
            _binding = null
        }
    }

    override fun onResume() {
        super.onResume()
        update()
    }
}