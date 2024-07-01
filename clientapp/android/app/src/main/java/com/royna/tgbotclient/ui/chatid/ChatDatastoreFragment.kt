package com.royna.tgbotclient.ui.chatid

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.snackbar.Snackbar
import com.royna.tgbotclient.databinding.FragmentDatastoreChatBinding
import com.royna.tgbotclient.datastore.ChatIDEntry
import com.royna.tgbotclient.util.DeviceUtils
import com.royna.tgbotclient.util.Logging
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@AndroidEntryPoint
class ChatDatastoreFragment : Fragment() {
    private var _binding: FragmentDatastoreChatBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    private val vm : ChatDatastoreViewModel by viewModels()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentDatastoreChatBinding.inflate(inflater, container, false)
        binding.datastoreSaveButton.setOnClickListener {
            if (vm.chatId.value == null || vm.chatName.value == null) {
                Snackbar.make(it, "Fill all fields", Snackbar.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val id = vm.chatId.value!!
            val name = vm.chatName.value!!
            vm.add(ChatIDEntry(id, name)).invokeOnCompletion { ex ->
                assert(ex == null)
                Snackbar.make(it, "Saved", Snackbar.LENGTH_SHORT).show()
                kAdapter.addItem(ChatTableViewItem(0, name, id))
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
            vm.clearAll().start()
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
        CoroutineScope(Dispatchers.IO).launch {
            vm.getAll().await().forEach {
                Logging.debug("Read: $it")
                withContext(Dispatchers.Main) {
                    kAdapter.addItem(ChatTableViewItem(chatName = it.name, chatId = it.id))

                }
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private lateinit var kAdapter : ChatTableViewAdapter
}