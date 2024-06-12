package com.royna.tgbotclient.ui.commands.uploadfile

import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import androidx.fragment.app.commit
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.snackbar.Snackbar
import com.royna.tgbotclient.R
import com.royna.tgbotclient.SocketCommandNative
import com.royna.tgbotclient.databinding.FragmentUploadFileBinding
import com.royna.tgbotclient.ui.CurrentSettingFragment

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
                    vm.setLiveData(it.toString())
                }
            }

        binding.uploadSelectFileButton.setOnClickListener {
            startForResult.launch(arrayOf("*/*"))
        }
        binding.uploadUploadButton.setOnClickListener {
            vm.execute(requireActivity(), object : SocketCommandNative.ICommandCallback {
                override fun onSuccess(result: Any?) {
                    Snackbar.make(it, "File uploaded", Snackbar.LENGTH_SHORT).show()
                }

                override fun onError(error: String) {
                    Snackbar.make(it, "File upload failed: $error", Snackbar.LENGTH_SHORT).show()
                }
            })
        }

        vm.liveData.observe(viewLifecycleOwner) {
            if (it.isNotEmpty()) {
                binding.uploadFileView.text = it
                binding.uploadUploadButton.isEnabled = true
            } else {
                binding.uploadFileView.text = resources.getString(R.string.no_file_selected)
                binding.uploadUploadButton.isEnabled = false
            }
        }

        return root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        childFragmentManager.commit {
            replace(R.id.current_setting_container, CurrentSettingFragment())
        }
    }
}