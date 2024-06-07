package com.royna.tgbotclient.ui.uptime

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import com.royna.tgbotclient.SocketCommandNative
import com.royna.tgbotclient.SocketCommandNative.getUptime
import com.royna.tgbotclient.databinding.FragmentUptimeBinding

class UptimeFragment : Fragment() {

    private var _binding: FragmentUptimeBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val uptimeViewModel =
            ViewModelProvider(this).get(UptimeViewModel::class.java)

        _binding = FragmentUptimeBinding.inflate(inflater, container, false)
        val root: View = binding.root

        val textView: TextView = binding.uptimeText
        uptimeViewModel.uptimeText.observe(viewLifecycleOwner) {
            textView.text = it
        }
        binding.uptimeRefreshBtn.setOnClickListener {
            uptimeViewModel.send()
        }
        return root
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}