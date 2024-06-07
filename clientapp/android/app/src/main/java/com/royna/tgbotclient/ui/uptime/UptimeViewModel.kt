package com.royna.tgbotclient.ui.uptime

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.royna.tgbotclient.SocketCommandNative
import com.royna.tgbotclient.SocketCommandNative.getUptime

class UptimeViewModel : ViewModel() {
    private var _uptimeText = MutableLiveData<String>().apply {
        value = ""
    }
    val uptimeText: LiveData<String> get() = _uptimeText

    fun send() {
        getUptime(object : SocketCommandNative.ICommandCallback {
            override fun onSuccess(result: Any?) {
                if (result is String) {
                    _uptimeText.value = result
                }
            }

            override fun onError(error: String) {
                _uptimeText.value = error
            }
        })
    }
}