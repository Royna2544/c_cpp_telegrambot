package com.royna.tgbotclient.ui.text_to_chat

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.Toast
import androidx.core.widget.addTextChangedListener
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import com.royna.tgbotclient.databinding.FragmentTextToChatBinding

class TextToChatFragment : Fragment() {
    private var _binding: FragmentTextToChatBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val TextToChatViewModel =
            ViewModelProvider(this)[TextToChatViewModel::class.java]

        _binding = FragmentTextToChatBinding.inflate(inflater, container, false)
        val root: View = binding.root

        val chatIdText: EditText = binding.chatIdText
        val messageText: EditText = binding.messageText
        TextToChatViewModel.chatId.observe(viewLifecycleOwner) {
            val str = when (it) {
                5185434015 -> "My brother"
                else -> "Destination Chat Id: $it"
            }
            binding.showChatIdText.text = str
        }
        TextToChatViewModel.messageText.observe(viewLifecycleOwner) {
            binding.showMessageText.text = "Message: $it"
        }
        chatIdText.addTextChangedListener { editor ->
            val id = runCatching {
                editor.toString().toLong()
            }.getOrDefault(0)
            TextToChatViewModel.setChatId(id)
        }
        messageText.doOnTextChanged { text, start, before, count ->
            TextToChatViewModel.setMessageText(text.toString())
        }
        binding.sendButton.setOnClickListener {
            val text = TextToChatViewModel.messageText.value
            if (text.isNullOrBlank()) {
                Toast.makeText(context, "Message is empty", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val chatId = TextToChatViewModel.chatId.value
            if (chatId == null) {
                Toast.makeText(context, "Chat Id is empty", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            TextToChatViewModel.send(chatId, text)
        }
        TextToChatViewModel.sendResult.observe(viewLifecycleOwner) {
            Toast.makeText(context, it, Toast.LENGTH_SHORT).show()
        }
        return root
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}