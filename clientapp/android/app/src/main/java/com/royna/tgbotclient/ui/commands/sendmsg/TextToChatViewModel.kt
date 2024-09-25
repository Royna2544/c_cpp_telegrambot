package com.royna.tgbotclient.ui.commands.sendmsg

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.royna.tgbotclient.SocketCommandNative
import com.royna.tgbotclient.datastore.IChatIDOperations
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
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

    private val gMainScope = CoroutineScope(Dispatchers.Main)
    private suspend fun sendAndWait(chatid: Long, message: String) : Unit =
        suspendCancellableCoroutine { continuation ->
            SocketCommandNative.sendWriteMessageToChatId(chatid, message,
                object : SocketCommandNative.ICommandStatusCallback {
                    override fun onStatusUpdate(status: SocketCommandNative.Status) {

                    }

                    override fun onSuccess(result: Any?) {
                    continuation.resumeWith(Result.success(Unit))
                }
                override fun onError(error: String) {
                    continuation.resumeWith(Result.failure(RuntimeException(error)))
                }
            })
        }

    fun send(chat: Long, message: String) {
        gMainScope.launch {
            var success = true
            try {
                withContext(Dispatchers.IO) {
                    sendAndWait(chat, message)
                }
            } catch (e: RuntimeException) {
                success = false
                _sendResult.value = "Failed: ${e.message}"
            }
            if (success) {
                _sendResult.value = "Message sent"
            }
        }
    }

    override fun onCleared() {
        super.onCleared()
        gMainScope.cancel()
    }

    fun getAll() = viewModelScope.async {
        withContext(Dispatchers.IO) {
            operation.getAll()
        }
    }
}