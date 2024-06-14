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
import androidx.lifecycle.ViewModelProvider
import com.royna.tgbotclient.R
import com.royna.tgbotclient.databinding.FragmentSendMessageBinding
import com.royna.tgbotclient.datastore.chat.SQLiteChatDatastore
import com.royna.tgbotclient.ui.CurrentSettingFragment

class TextToChatFragment : Fragment() {
    private var _binding: FragmentSendMessageBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val textToChatViewModel =
            ViewModelProvider(this)[TextToChatViewModel::class.java]

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
            binding.showChatIdText.text = if (mChatIdMap.containsValue(it)) {
                getString(R.string.destination_chat_fmt, mChatIdMap.filterValues {
                    v-> v == it
                }.keys.first(), it)
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
                mChatIdMap[editor.toString()].let {
                    if (it != null) {
                        // Found
                        textToChatViewModel.setChatId(it)
                    } else {
                        // Not found
                        mChatIdMap.filterKeys { key ->
                            key.lowercase() == editor.toString().lowercase()
                        }.also { map ->
                            assert(map.size == 1 || map.isEmpty())
                            if (map.isEmpty()) {
                                textToChatViewModel.setChatId(InvalidChatId)
                            }
                        }.forEach { entry ->
                            textToChatViewModel.setChatId(entry.value)
                        }
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

        mChatIdMap = SQLiteChatDatastore(requireContext()).readAll()
        childFragmentManager.commit {
            replace(R.id.current_setting_container, CurrentSettingFragment())
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private var mChatIdMap: Map<String, Long> = mapOf()
    companion object {
        private const val InvalidChatId = 0L
    }
}