package com.royna.tgbotclient.ui.commands.uploadfile

import android.content.SharedPreferences
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import androidx.fragment.app.commit
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.preference.PreferenceManager
import com.royna.tgbotclient.R
import com.royna.tgbotclient.databinding.FragmentUploadFileBinding
import com.royna.tgbotclient.net.SocketContext
import com.royna.tgbotclient.ui.CurrentSettingFragment
import com.royna.tgbotclient.util.FileUtils.queryFileName
import kotlinx.coroutines.launch

class UploadFileFragment : Fragment() {
    private var _binding: FragmentUploadFileBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentUploadFileBinding.inflate(inflater, container, false)
        val vm = ViewModelProvider(this)[UploadFileViewModel::class.java]
        val root: View = binding.root
        val startForResult: ActivityResultLauncher<Array<String>> =
            registerForActivityResult(ActivityResultContracts.OpenDocument()) {
                if (it != null) {
                    vm.setFileUri(it)
                    queryFileName(requireActivity().contentResolver, it)
                        ?.let { filename ->
                            binding.uploadFileView.text = filename
                        }
                    binding.uploadUploadButton.isEnabled = true
                } else {
                    binding.uploadFileView.text = resources.getString(R.string.no_file_selected)
                    binding.uploadUploadButton.isEnabled = false
                }
            }
        binding.uploadFileView.text = getString(R.string.no_file_selected)
        binding.uploadSelectFileButton.setOnClickListener {
            startForResult.launch(arrayOf("*/*"))
        }
        binding.uploadUploadButton.setOnClickListener {
            vm.execute(requireActivity())
        }
        binding.uploadUploadButton.isEnabled = false
        val pref = PreferenceManager.getDefaultSharedPreferences(requireContext())
        fun updateOptionAndPref(option: SocketContext.UploadOption) {
            SocketContext.getInstance().setUploadFileOptions(option)
            pref.edit().putInt(UploadOptionPref, option.value).apply()
        }
        // Allowance level 1
        binding.uploadOption1.setOnClickListener {
            updateOptionAndPref(SocketContext.UploadOption.MUST_NOT_EXIST)
        }
        // Allowance level 2
        binding.uploadOption2.setOnClickListener {
            updateOptionAndPref(SocketContext.UploadOption.MUST_NOT_MATCH_CHECKSUM)
        }
        // Allowance level 3
        binding.uploadOption3.setOnClickListener {
            updateOptionAndPref(SocketContext.UploadOption.ALWAYS)
        }
        val kBindingMap = mapOf(
            SocketContext.UploadOption.MUST_NOT_EXIST to binding.uploadOption1,
            SocketContext.UploadOption.MUST_NOT_MATCH_CHECKSUM to binding.uploadOption2,
            SocketContext.UploadOption.ALWAYS to binding.uploadOption3
        )
        pref.getUploadOption(UploadOptionPref, SocketContext.UploadOption.MUST_NOT_EXIST).let { num ->
            kBindingMap[num]?.isChecked = true
            updateOptionAndPref(num)
        }
        lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                vm.uploadResult.collect { message ->
                    Toast.makeText(requireContext(), message, Toast.LENGTH_SHORT).show()
                }
            }
        }
        return root
    }

    private fun SharedPreferences.getUploadOption(key: String, default: SocketContext.UploadOption):
            SocketContext.UploadOption {
        return getInt(key, default.value).let {
            SocketContext.UploadOption.entries.find { ent ->
                ent.value == it
            }
        } ?: default
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

    companion object {
        const val UploadOptionPref = "upload_option"
    }
}