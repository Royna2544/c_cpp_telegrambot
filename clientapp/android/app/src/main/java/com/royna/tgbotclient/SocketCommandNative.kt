@file:Suppress("KotlinJniMissingFunction")

package com.royna.tgbotclient

object SocketCommandNative {
    // No effect but sync with <arpa/inet.h>
    enum class DestinationType (val af: Int) {
        IPv4(2),
        IPv6(10)
    }

    /**
     * Status of the command
     * Used in ICommandStatusCallback.onStatusUpdate
     * Sync with native TgBotSocketNative::Callbacks::Status
     */
    enum class Status(val value: Int) {
        INVALID(0),
        CONNECTION_PREPARED(1),
        CONNECTED(2),
        HEADER_PACKET_SENT(3),
        DATA_PACKET_SENT(4),
        HEADER_PACKET_RECEIVED(5),
        DATA_PACKET_RECEIVED(6),
        PROCESSED_DATA(7),
        DONE(8),
    }

    enum class UploadOption(val value: Int) {
        MUST_NOT_EXIST(1),
        MUST_NOT_MATCH_CHECKSUM(2),
        ALWAYS(3)
    }

    // Generic interface for command callbacks
    interface ICommandCallback {
        /***
         * Called when the command is executed successfully
         *
         * @param result The result of the command. Different values per-command
         */
        fun onSuccess(result: Any?)

        /***
         * Called when the command fails
         *
         * @param error The error message
         */
        fun onError(error: String)
    }

    // Extension of ICommandCallback for status updates
    interface ICommandStatusCallback : ICommandCallback {
        /***
         * Called when the status of the command changes
         *
         * @param status The new status
         */
        fun onStatusUpdate(status: Status)

        // Alias function, native code calls this instead of onStatusUpdate above.
        fun onStatusUpdate(status: Int) {
            onStatusUpdate(Status.entries[status])
        }
    }

    data class DestinationInfo(var ipaddr: String, var type: DestinationType, var port: Int)

    // Calls the native code to change the destination
    fun changeDestinationInfo(ipaddr: String, type: DestinationType, port: Int) {
        changeDestinationInfo(ipaddr, type.af, port)
    }

    // Set the upload file options via a call to native code
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

    // Commands
    external fun sendWriteMessageToChatId(chatId: Long, text: String, callback: ICommandStatusCallback)
    external fun getUptime(callback: ICommandStatusCallback)
    external fun uploadFile(path: String, destFilePath: String, callback: ICommandStatusCallback)
    external fun downloadFile(remoteFilePath: String, localFilePath: String, callback: ICommandStatusCallback)

    // Option configuration
    private external fun setUploadFileOptions(failIfExist : Boolean, failIfChecksumMatch : Boolean)
    private external fun changeDestinationInfo(ipaddr: String, type: Int, port: Int) : Boolean
    external fun getCurrentDestinationInfo() : DestinationInfo

    // Initialize abseil logging
    private external fun initLogging()

    init {
        System.loadLibrary("tgbotclient")
        initLogging()
    }
}