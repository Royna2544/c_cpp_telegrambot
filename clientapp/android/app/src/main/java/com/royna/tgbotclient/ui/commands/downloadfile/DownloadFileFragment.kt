package com.royna.tgbotclient.ui.commands.downloadfile

import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.widget.doAfterTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.commit
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.MediatorLiveData
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.royna.tgbotclient.R
import com.royna.tgbotclient.databinding.FragmentDownloadFileBinding
import com.royna.tgbotclient.ui.CurrentSettingFragment
import com.royna.tgbotclient.util.FileUtils.dirJoin
import kotlinx.coroutines.launch
import java.io.File

class DownloadFileFragment : Fragment() {
    private var _binding: FragmentDownloadFileBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentDownloadFileBinding.inflate(inflater, container, false)
        val vm = ViewModelProvider(this)[DownloadFileViewModel::class.java]
        val root: View = binding.root
        val startForResult =
            registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) {
                if (it != null) {
                    vm.setFileUrl(it)
                }
            }
        binding.downloadSelectFileButton.setOnClickListener {
            startForResult.launch(null)
        }
        binding.downloadDownloadButton.setOnClickListener {
            vm.execute(requireActivity())
        }
        binding.downloadFileName.doAfterTextChanged {
            vm.setSourceFile(it.toString())
        }
        // Default values
        binding.downloadFileView.text = resources.getString(R.string.no_file_selected)
        binding.downloadDownloadButton.isEnabled = false

        // Observe the Uri and Filename
        val mediatorLiveData = MediatorLiveData<Pair<Uri?, String?>>()
        mediatorLiveData.addSource(vm.outFileURI) { value1 ->
            mediatorLiveData.value = Pair(value1, vm.sourceFilePath.value)
        }
        mediatorLiveData.addSource(vm.sourceFilePath) { value2 ->
            mediatorLiveData.value = Pair(vm.outFileURI.value, value2)
        }
        mediatorLiveData.observe(viewLifecycleOwner) { pair ->
            // Use both values here
            val value1 = pair.first
            val value2 = pair.second
            // Handle the combined values as needed
            if (value1 != null && !value2.isNullOrEmpty()) {
                binding.downloadDownloadButton.isEnabled = true
            } else {
                binding.downloadDownloadButton.isEnabled = false
            }

            // Folder name
            val dirname = (value1?.path?.substringAfter(':'))?.let {
                listOf(Environment.getExternalStorageDirectory().path, it).dirJoin()
            }
            // Detect if the target file already exist
            val overwriting = if (value2?.let { dirname?.let { dir -> File(dir, it).isFile } } == true) {
                "Overwriting existing file"
            } else null

            binding.downloadFileView.text = listOfNotNull(value2, dirname, overwriting).joinToString("\n")
        }

        lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                vm.downloadEvent.collect { message ->
                    Toast.makeText(requireContext(), message, Toast.LENGTH_SHORT).show()
                }
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

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}