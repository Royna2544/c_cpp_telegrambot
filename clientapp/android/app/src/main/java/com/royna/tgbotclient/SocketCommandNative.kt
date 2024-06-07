@file:Suppress("KotlinJniMissingFunction")

package com.royna.tgbotclient

object SocketCommandNative {
    // No effect but sync with <arpa/inet.h>
    enum class DestinationType (val af: Int) {
        IPv4(2),
        IPv6(10)
    }
    fun changeDestinationInfo(ipaddr: String, type: DestinationType) {
        changeDestinationInfo(ipaddr, type.af)
    }

    interface ICommandCallback {
        fun onSuccess(result: Any?)
        fun onError(error: String)
    }

    external fun sendWriteMessageToChatId(chatId: Long, text: String, callback: ICommandCallback)
    external fun getUptime(callback: ICommandCallback)
    external fun uploadFile(path: String, destFilePath: String, callback: ICommandCallback)
    external fun downloadFile(remoteFilePath: String, localFilePath: String, callback: ICommandCallback)
    private external fun changeDestinationInfo(ipaddr: String, type: Int) : Boolean
    private external fun initLogging()

    init {
        System.loadLibrary("tgbotclient")
        initLogging()
    }
}