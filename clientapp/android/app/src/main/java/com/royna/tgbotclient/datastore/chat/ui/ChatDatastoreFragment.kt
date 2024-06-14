package com.royna.tgbotclient.datastore.chat.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.snackbar.Snackbar
import com.royna.tgbotclient.databinding.FragmentDatastoreChatBinding
import com.royna.tgbotclient.datastore.chat.SQLiteChatDatastore
import com.royna.tgbotclient.util.DeviceUtils
import com.royna.tgbotclient.util.Logging

class ChatDatastoreFragment : Fragment() {
    private var _binding: FragmentDatastoreChatBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val vm = ViewModelProvider(this)[ChatDatastoreViewModel::class.java]
        _binding = FragmentDatastoreChatBinding.inflate(inflater, container, false)
        kDatastore = SQLiteChatDatastore(requireContext())
        binding.datastoreSaveButton.setOnClickListener {
            if (vm.chatId.value == null || vm.chatName.value == null) {
                Snackbar.make(it, "Fill all fields", Snackbar.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            if (kDatastore.write(vm.chatName.value!!, vm.chatId.value!!)) {
                Snackbar.make(it, "Saved", Snackbar.LENGTH_SHORT).show()
                kAdapter.addItem(ChatTableViewItem(0, vm.chatName.value!!, vm.chatId.value!!))
            } else {
                Snackbar.make(it, "Failed", Snackbar.LENGTH_SHORT).show()
            }
        }
        binding.datastoreChatEdit.doOnTextChanged { text, _, _, _ ->
            runCatching {
                text.toString().toLong()
            }.onSuccess {
                vm.setChatId(it)
            }
        }
        binding.datastoreNameEdit.doOnTextChanged { text, _, _, _ ->
            vm.setChatName(text.toString())
        }
        binding.datastoreClearButton.setOnClickListener {
            kDatastore.clearAll()
            kAdapter.clearAll()
        }
        kAdapter = ChatTableViewAdapter()
        binding.datastoreChatRecyclerView.layoutManager = LinearLayoutManager(requireContext())
        binding.datastoreChatRecyclerView.adapter = kAdapter
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        (DeviceUtils.getScreenWidth(requireActivity()) / 2).let {
            Logging.debug("Adapter width: $it")
            requireView().post {
                binding.datastoreChatEdit.width = it
                binding.datastoreNameEdit.width = it
            }
        }
        kAdapter.clearAll()
        kDatastore.readAll().forEach {
            Logging.debug("Read: $it")
            kAdapter.addItem(ChatTableViewItem(chatName = it.key, chatId = it.value))
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private lateinit var kDatastore : SQLiteChatDatastore
    private lateinit var kAdapter : ChatTableViewAdapter
}