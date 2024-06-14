@file:Suppress("KotlinJniMissingFunction")

package com.royna.tgbotclient

object SocketCommandNative {
    // No effect but sync with <arpa/inet.h>
    enum class DestinationType (val af: Int) {
        IPv4(2),
        IPv6(10)
    }

    enum class UploadOption(val value: Int) {
        MUST_NOT_EXIST(1),
        MUST_NOT_MATCH_CHECKSUM(2),
        ALWAYS(3)
    }

    interface ICommandCallback {
        fun onSuccess(result: Any?)
        fun onError(error: String)
    }
    data class DestinationInfo(var ipaddr: String, var type: DestinationType, var port: Int)

    fun changeDestinationInfo(ipaddr: String, type: DestinationType, port: Int) {
        changeDestinationInfo(ipaddr, type.af, port)
    }
    fun setUploadFileOptions(options: UploadOption) {
        when (options) {
            UploadOption.MUST_NOT_EXIST -> setUploadFileOptions(
                failIfExist = true,
                failIfChecksumMatch = false
            )
            UploadOption.MUST_NOT_MATCH_CHECKSUM -> setUploadFileOptions(
                failIfExist = false,
                failIfChecksumMatch = true
            )
            UploadOption.ALWAYS -> setUploadFileOptions(
                failIfExist = false,
                failIfChecksumMatch = false
            )
        }
    }

    external fun sendWriteMessageToChatId(chatId: Long, text: String, callback: ICommandCallback)
    external fun getUptime(callback: ICommandCallback)
    external fun uploadFile(path: String, destFilePath: String, callback: ICommandCallback)
    private external fun setUploadFileOptions(failIfExist : Boolean, failIfChecksumMatch : Boolean)
    external fun downloadFile(remoteFilePath: String, localFilePath: String, callback: ICommandCallback)
    private external fun changeDestinationInfo(ipaddr: String, type: Int, port: Int) : Boolean
    external fun getCurrentDestinationInfo() : DestinationInfo

    private external fun initLogging()

    init {
        System.loadLibrary("tgbotclient")
        initLogging()
    }
}