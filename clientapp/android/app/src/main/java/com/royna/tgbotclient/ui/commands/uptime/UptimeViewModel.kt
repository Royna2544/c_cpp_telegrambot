package com.royna.tgbotclient.ui.commands.uptime

import androidx.fragment.app.FragmentActivity
import com.royna.tgbotclient.R
import com.royna.tgbotclient.SocketCommandNative
import com.royna.tgbotclient.SocketCommandNative.getUptime
import com.royna.tgbotclient.ui.commands.SingleViewModelBase
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import kotlin.coroutines.resumeWithException

class UptimeViewModel : SingleViewModelBase<String, String>() {

    override suspend fun coroutineFunction(activity: FragmentActivity): String = suspendCancellableCoroutine {
        getUptime(object : SocketCommandNative.ICommandCallback {
            override fun onSuccess(result: Any?) {
                if (result is String) {
                    it.resumeWith(Result.success(result))
                } else {
                    it.resumeWithException(AssertionError("Unknown result type"))
                }
            }

            override fun onError(error: String) {
                it.resumeWithException(RuntimeException(error))
            }
        })
    }
    override fun execute(
        activity: FragmentActivity,
        callback: SocketCommandNative.ICommandCallback
    ) {
        gMainScope.launch {
            val result : String
            _liveData.postValue(activity.getString(R.string.get_wip))
            try {
                withContext(Dispatchers.IO) {
                    result = coroutineFunction(activity)
                }
                callback.onSuccess(result)
            } catch (e: Exception) {
                callback.onError(e.message.toString())
            }
        }
    }
}