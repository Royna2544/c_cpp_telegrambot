package com.royna.tgbotclient.datastore.chat.ui

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.royna.tgbotclient.datastore.chat.ChatId

class ChatDatastoreViewModel : ViewModel() {
    private var _chatId = MutableLiveData<ChatId>()
    private var _chatName = MutableLiveData<String>()
    val chatId: LiveData<ChatId> = _chatId
    val chatName: LiveData<String> = _chatName
    fun setChatId(chatId: ChatId) {
        _chatId.value = chatId
    }
    fun setChatName(chatName: String) {
        _chatName.value = chatName
    }
}