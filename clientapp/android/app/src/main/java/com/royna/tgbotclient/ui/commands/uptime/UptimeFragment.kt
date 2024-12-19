package com.royna.tgbotclient.ui.commands.uptime

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.fragment.app.commit
import androidx.lifecycle.ViewModelProvider
import com.royna.tgbotclient.R
import com.royna.tgbotclient.databinding.FragmentUptimeBinding
import com.royna.tgbotclient.ui.CurrentSettingFragment

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
            ViewModelProvider(this)[UptimeViewModel::class.java]

        _binding = FragmentUptimeBinding.inflate(inflater, container, false)
        val root: View = binding.root

        val textView: TextView = binding.uptimeText
        uptimeViewModel.uptimeValue.observe(viewLifecycleOwner) {
            textView.text = it
        }
        uptimeViewModel.fetchInProgress.observe(viewLifecycleOwner) {
            binding.uptimeRefreshBtn.isEnabled = !it
            binding.uptimeRefreshBtn.text = if (it) getString(R.string.get_wip) else getString(R.string.get)
        }
        binding.uptimeRefreshBtn.setOnClickListener {
            uptimeViewModel.execute()
        }
        textView.text = getString(R.string.get_desc)
        return root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        childFragmentManager.commit {
            replace(R.id.current_setting_container, CurrentSettingFragment())
        }
    }
    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}