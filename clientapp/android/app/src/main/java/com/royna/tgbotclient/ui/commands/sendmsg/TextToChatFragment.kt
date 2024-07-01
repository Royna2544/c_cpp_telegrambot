package com.royna.tgbotclient.ui.commands.sendmsg

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.Toast
import androidx.core.widget.doAfterTextChanged
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.commit
import androidx.fragment.app.viewModels
import com.royna.tgbotclient.R
import com.royna.tgbotclient.databinding.FragmentSendMessageBinding
import com.royna.tgbotclient.datastore.ChatIDEntry
import com.royna.tgbotclient.ui.CurrentSettingFragment
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

@AndroidEntryPoint
class TextToChatFragment : Fragment() {
    private var _binding: FragmentSendMessageBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    private val textToChatViewModel : TextToChatViewModel by viewModels()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {

        _binding = FragmentSendMessageBinding.inflate(inflater, container, false)
        val root: View = binding.root

        val chatIdText: EditText = binding.chatIdText
        val messageText: EditText = binding.messageText

        fun computeSendButtonState() {
            val chatIdValue = textToChatViewModel.chatId.value
            val messageTextValue = textToChatViewModel.messageText.value
            binding.sendButton.isEnabled = chatIdValue.let {
                (it != null) && (it != InvalidChatId)
            }  && !messageTextValue.isNullOrBlank()
        }

        textToChatViewModel.chatId.observe(viewLifecycleOwner) {
            val chatName = mChatIdMap.find { ent ->
                ent.id == it
            }?.name
            binding.showChatIdText.text = if (chatName != null) {
                getString(R.string.destination_chat_fmt, chatName, it)
            } else if (it != InvalidChatId) {
                getString(R.string.destination_chat_id_fmt, it)
            } else {
                getString(R.string.destination_chat)
            }
        }
        textToChatViewModel.messageText.observe(viewLifecycleOwner) {
            computeSendButtonState()
            binding.showMessageText.text = if (!it.isNullOrBlank())
                getString(R.string.message_fmt, it) else getString(R.string.message_text)
        }
        chatIdText.doAfterTextChanged { editor ->
            runCatching {
                editor.toString().toLong()
            }.onSuccess {
                textToChatViewModel.setChatId(it)
                computeSendButtonState()
            }.onFailure {
                // This is not a number, query it on DB
                mChatIdMap.filter {
                    it.name.lowercase() == editor.toString().lowercase()
                }.let { map ->
                    if (map.isNotEmpty()) {
                        // Found
                        assert(map.size == 1)
                        textToChatViewModel.setChatId(map.first().id)
                    } else {
                        // Not found
                        textToChatViewModel.setChatId(InvalidChatId)
                    }
                    computeSendButtonState()
                }
            }
        }
        messageText.doOnTextChanged { text, _, _, _ ->
            textToChatViewModel.setMessageText(text.toString())
        }
        binding.sendButton.isEnabled = false
        binding.sendButton.setOnClickListener {
            val text = textToChatViewModel.messageText.value
            if (text.isNullOrBlank()) {
                Toast.makeText(context, "Message is empty", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val chatId = textToChatViewModel.chatId.value
            if (chatId == null) {
                Toast.makeText(context, "Chat Id is empty", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            textToChatViewModel.send(chatId, text)
        }
        textToChatViewModel.sendResult.observe(viewLifecycleOwner) {
            Toast.makeText(context, it, Toast.LENGTH_SHORT).show()
        }
        return root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        CoroutineScope(Dispatchers.IO).launch {
            mChatIdMap = textToChatViewModel.getAll().await()
        }
        childFragmentManager.commit {
            replace(R.id.current_setting_container, CurrentSettingFragment())
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private var mChatIdMap: List<ChatIDEntry> = listOf()
    companion object {
        private const val InvalidChatId = 0L
    }
}