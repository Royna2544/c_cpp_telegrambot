package com.royna.tgbotclient.ui.chatid

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.royna.tgbotclient.datastore.ChatID
import com.royna.tgbotclient.datastore.ChatIDEntry
import com.royna.tgbotclient.datastore.IChatIDOperations
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import javax.inject.Inject

@HiltViewModel
class ChatDatastoreViewModel @Inject constructor(private val operation: IChatIDOperations) : ViewModel() {
    private var _chatId = MutableLiveData<ChatID>()
    private var _chatName = MutableLiveData<String>()
    val chatId: LiveData<ChatID> = _chatId
    val chatName: LiveData<String> = _chatName
    fun setChatId(chatId: ChatID) {
        _chatId.value = chatId
    }
    fun setChatName(chatName: String) {
        _chatName.value = chatName
    }
    fun clearAll() = viewModelScope.launch {
        withContext(Dispatchers.IO) {
            operation.clearAll()
        }
    }
    fun add(entry: ChatIDEntry) = viewModelScope.launch {
        withContext(Dispatchers.IO) {
            operation.add(entry)
        }
    }
    fun getAll() = viewModelScope.async {
        withContext(Dispatchers.IO) {
            operation.getAll()
        }
    }
}