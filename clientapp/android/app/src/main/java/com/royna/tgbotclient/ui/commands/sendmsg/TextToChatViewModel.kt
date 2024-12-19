package com.royna.tgbotclient.ui.commands.sendmsg

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.royna.tgbotclient.datastore.IChatIDOperations
import com.royna.tgbotclient.net.SocketContext
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import javax.inject.Inject

@HiltViewModel
class TextToChatViewModel @Inject constructor(private val operation: IChatIDOperations) : ViewModel() {
    // Private MutableLiveData
    private val _messageText = MutableLiveData<String>()
    private val _chatId = MutableLiveData<Long>()
    private val _sendResult = MutableLiveData<String>()

    // Public immutable LiveData
    val messageText: LiveData<String> get() = _messageText
    val chatId: LiveData<Long> get() = _chatId
    val sendResult: LiveData<String> get() = _sendResult

    // Methods to update LiveData
    fun setChatId(chatId: Long) {
        _chatId.value = chatId
    }

    fun setMessageText(messageText: String) {
        _messageText.value = messageText
    }

    fun send(chat: Long, message: String) {
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                SocketContext.getInstance().sendMessage(chat, message)
            }.onSuccess {
                _sendResult.value = "Message sent"
            }.onFailure { e ->
                _sendResult.value = "Failed: ${e.message}"
            }
        }
    }

    fun getAll() = viewModelScope.async {
        withContext(Dispatchers.IO) {
            operation.getAll()
        }
    }
}