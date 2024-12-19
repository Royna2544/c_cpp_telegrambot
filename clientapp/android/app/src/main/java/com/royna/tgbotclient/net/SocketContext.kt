package com.royna.tgbotclient.net

import com.google.gson.Gson
import com.royna.tgbotclient.datastore.ChatID
import com.royna.tgbotclient.net.crypto.SHAPartial
import com.royna.tgbotclient.net.data.Command
import com.royna.tgbotclient.net.data.GenericAck
import com.royna.tgbotclient.net.data.GenericAckException
import com.royna.tgbotclient.net.data.Packet
import com.royna.tgbotclient.net.data.PayloadType
import com.royna.tgbotclient.util.Logging
import io.ktor.network.selector.SelectorManager
import io.ktor.network.sockets.Connection
import io.ktor.network.sockets.InetSocketAddress
import io.ktor.network.sockets.aSocket
import io.ktor.network.sockets.connection
import io.ktor.utils.io.read
import io.ktor.utils.io.writeFully
import kotlinx.coroutines.Dispatchers
import java.io.File
import java.nio.ByteBuffer

class SocketContext {
    private suspend fun openConnection() : Connection {
        val selectorManager = SelectorManager(Dispatchers.IO)
        val serverSocket = aSocket(selectorManager).tcp().connect(mAddress)
        Logging.info("Waiting connection on $mAddress")
        val socket = serverSocket.connection()
        Logging.info("Connected")
        return socket
    }

    private suspend fun doOpenSession(channels: Connection) = runCatching {
        Logging.debug("Opened connection")
        var mSessionToken = String(ByteArray(Packet.SESSION_TOKEN_LENGTH))
        val openSessionPacket = Packet.create(
            command = Command.CMD_OPEN_SESSION,
            payloadType = PayloadType.Binary,
            sessionToken = mSessionToken
        ).getOrElse {
            Logging.error("Failed to create packet", it)
            throw RuntimeException("Failed to create packet")
        }

        channels.output.writeFully(openSessionPacket)
        channels.output.flush()
        Logging.debug("Wrote CMD_OPEN_SESSION")

        lateinit var header : Packet
        channels.input.read(Packet.PACKET_HEADER_LEN) { bytebuffer ->
            header = Packet.fromByteBuffer(bytebuffer).getOrThrow()
            if (header.command != Command.CMD_OPEN_SESSION_ACK) {
                Logging.error("Invalid response: ${header.command}")
                throw RuntimeException("Invalid response")
            }
            Logging.debug("Read CMD_OPEN_SESSION_ACK")
            mSessionToken = header.sessionToken
            Logging.debug("Got session token: $mSessionToken")
        }
        // Process payload
        channels.input.read(Packet.PACKET_HEADER_LEN) { bytebuffer ->
            Packet.readPayload(bytebuffer, header).onSuccess {
                header.payload = it
            }.getOrThrow()
        }
        Logging.info("Opened session")
        mSessionToken
    }

    private suspend fun closeSession(channels: Connection, mSessionToken: String) {
        val closeSessionPacket = Packet.create(
            command = Command.CMD_CLOSE_SESSION,
            payloadType = PayloadType.Binary,
            sessionToken = mSessionToken
        ).getOrElse {
            Logging.error("Failed to create packet", it)
            throw RuntimeException("Failed to create packet")
        }
        channels.output.writeFully(closeSessionPacket)
        channels.output.flush()
        Logging.debug("Wrote CMD_CLOSE_SESSION")
    }

    private suspend fun readGenericAck(channels: Connection) = runCatching {
        lateinit var header : Packet
        channels.input.read(Packet.PACKET_HEADER_LEN) { bytebuffer ->
            header = Packet.fromByteBuffer(bytebuffer).getOrThrow()
            require(header.command == Command.CMD_GENERIC_ACK) {
                "Invalid response: ${header.command}"
            }
        }
        Logging.debug("Read generic ack")

        lateinit var payload : ByteArray
        // Process payload
        channels.input.read(header.length) { bytebuffer ->
            Packet.readPayload(bytebuffer, header).onSuccess {
                payload = it
            }.getOrThrow()
        }
        Logging.debug("Payload: ${payload.decodeToString()}")
        GenericAck.fromJson(payload.decodeToString())
    }

    enum class UploadOption(val value: Int) {
        MUST_NOT_EXIST(1),
        MUST_NOT_MATCH_CHECKSUM(2),
        ALWAYS(3)
    }

    data class SendMessageData(val chat: ChatID, val message: String)

    suspend fun sendMessage(chatId: Long, message: String) = runCatching {
        val channels = openConnection()
        val sessionToken = doOpenSession(channels).onFailure {
            Logging.error("Failed to open session", it)
        }.getOrThrow()

        val sendMessagePacket = Packet.create(
            Command.CMD_WRITE_MSG_TO_CHAT_ID,
            PayloadType.Json,
            sessionToken,
            Gson().toJson(SendMessageData(chatId, message)).toByteArray())
        .getOrElse {
            Logging.error("Failed to create packet", it)
            throw it
        }
        channels.output.writeFully(sendMessagePacket)
        channels.output.flush()
        Logging.debug("Wrote CMD_SEND_FILE_TO_CHAT_ID")
        readGenericAck(channels).onSuccess {
            if (!it.success()) {
                throw GenericAckException(it)
            }
        }.onFailure {
            Logging.error("Failed to read generic ack", it)
        }.getOrThrow()
        closeSession(channels, sessionToken)
    }

    data class GetUptimeData(val start_time: String, val current_time: String, val uptime: String)
    suspend fun getUptime() = runCatching {
         val channels = openConnection()
         val sessionToken = doOpenSession(channels).onFailure {
             Logging.error("Failed to open session", it)
         }.getOrThrow()

         val sendMessagePacket = Packet.create(
             Command.CMD_GET_UPTIME,
             PayloadType.Json,
             sessionToken
         ).getOrElse {
             Logging.error("Failed to create packet", it)
             throw it
         }
         channels.output.writeFully(sendMessagePacket)
         channels.output.flush()
         Logging.debug("Wrote CMD_GET_UPTIME")

         var result: GetUptimeData? = null
         // Read the ACK
         runCatching {
             lateinit var header : Packet
             channels.input.read(Packet.PACKET_HEADER_LEN) { bytebuffer ->
                 header = Packet.fromByteBuffer(bytebuffer).getOrThrow()
                 require(header.command == Command.CMD_GET_UPTIME_CALLBACK) {
                     "Invalid response: ${header.command}"
                 }
             }
             Logging.debug("Read ${header.command}")

             lateinit var payload : ByteArray
             channels.input.read(header.length) { bytebuffer ->
                 Packet.readPayload(bytebuffer, header).onSuccess {
                     payload = it
                 }.getOrThrow()
             }
             val str = payload.decodeToString()
             Logging.debug("Payload: $str")
             result = Gson().fromJson(str, GetUptimeData::class.java)
             if (result == null) {
                 Logging.error("Failed to parse CMD_GET_UPTIME ack")
                 throw RuntimeException("Failed to parse CMD_GET_UPTIME ack")
             }
         }.onFailure {
             Logging.error("Failed to read CMD_GET_UPTIME ack", it)
         }.getOrThrow()
         closeSession(channels, sessionToken)
         result?.uptime
    }

    data class TransferFileData(val destfilepath: String, val srcfilepath: String, val hash: String,
                                val options: Options) {
        data class Options (
            val overwrite: Boolean,
            val hash_ignore: Boolean,
            var dry_run : Boolean
        )
    }
    data class TransferUploadData(val destfilepath: String, val srcfilepath: String, val options: Options) {
        data class Options (
            val overwrite: Boolean,
            val hash_ignore: Boolean,
            var dry_run : Boolean
        )
    }

    suspend fun uploadFile(sourcePath: File, destPath: String) = runCatching {
        val channels = openConnection()
        val sessionToken = doOpenSession(channels).onFailure {
            Logging.error("Failed to open session", it)
        }.getOrThrow()

        Logging.debug("File size: ${sourcePath.length()}")

        val (overwrite, hash_ignore) = when (mUploadOption) {
            UploadOption.MUST_NOT_EXIST -> Pair(false, false)
            UploadOption.MUST_NOT_MATCH_CHECKSUM -> Pair(true, false)
            UploadOption.ALWAYS -> Pair(true, true)
        }
        val fileBuf = sourcePath.readBytes()
        val hash = SHAPartial().create(fileBuf).toString()
        val data = TransferFileData(
            destfilepath = destPath,
            srcfilepath = sourcePath.absolutePath,
            hash = hash,
            options = TransferFileData.Options(
                overwrite = overwrite,
                hash_ignore = hash_ignore,
                dry_run = true
            )
        )

        val uploadDryPacket = Packet.create(
            Command.CMD_TRANSFER_FILE,
            PayloadType.Json,
            sessionToken,
            Gson().toJson(data).toByteArray().also {
                Logging.debug("Payload: ${it.decodeToString()}")
            }
        )

        channels.output.writeFully(uploadDryPacket.getOrElse {
            Logging.error("Failed to create CMD_TRANSFER_FILE packet", it)
            throw it
        })
        channels.output.flush()
        Logging.debug("Wrote CMD_TRANSFER_FILE")

        readGenericAck(channels).onSuccess {
            if (it.success()) {
                data.options.dry_run = false
                val jsonPayload = Gson().toJson(data).toByteArray()
                Logging.debug("Payload: ${jsonPayload.decodeToString()}, size: ${jsonPayload.size}")
                val uploadPkt = Packet.create(
                    Command.CMD_TRANSFER_FILE,
                    PayloadType.Json,
                    sessionToken,
                    ByteBuffer.allocate(jsonPayload.size + fileBuf.size + 1).apply {
                        put(jsonPayload)
                        put(0xFFu.toByte())
                        put(fileBuf)
                    }.array()
                ).getOrElse { thr ->
                    Logging.error("Failed to create CMD_TRANSFER_FILE packet", thr)
                    throw thr
                }
                channels.output.writeFully(uploadPkt)
                channels.output.flush()
                Logging.debug("Wrote CMD_TRANSFER_FILE")
                readGenericAck(channels).onFailure { fail ->
                    Logging.error("Failed to read generic ack", fail)
                    throw fail
                }
            } else {
                throw GenericAckException(it)
            }
        }.onFailure {
            Logging.error("Failed to read generic ack", it)
            throw it
        }
        closeSession(channels, sessionToken)
    }

    suspend fun downloadFile(sourcePath: String, destPath: String) = runCatching {
        val channels = openConnection()
        val sessionToken = doOpenSession(channels).onFailure {
            Logging.error("Failed to open session", it)
        }.getOrThrow()

        val data = TransferUploadData(
            destfilepath = destPath,
            srcfilepath = sourcePath,
            options = TransferUploadData.Options(
                overwrite = false,
                hash_ignore = true,
                dry_run = true
            )
        )

        val transferRequest = Packet.create(
            Command.CMD_TRANSFER_FILE_REQUEST,
            PayloadType.Json,
            sessionToken,
            Gson().toJson(data).toByteArray().also {
                Logging.debug("Payload: ${it.decodeToString()}")
            }
        )
        channels.output.writeFully(transferRequest.getOrElse {
            Logging.error("Failed to create CMD_TRANSFER_FILE_REQUEST packet", it)
            throw it
        })
        channels.output.flush()
        Logging.debug("Wrote CMD_TRANSFER_FILE_REQUEST")

        lateinit var header : Packet
        channels.input.read(Packet.PACKET_HEADER_LEN) { bytebuffer ->
            header = Packet.fromByteBuffer(bytebuffer).getOrThrow()
            require(header.command == Command.CMD_TRANSFER_FILE || header.command == Command.CMD_GENERIC_ACK) {
                "Invalid response: ${header.command}"
            }
        }
        Logging.debug("Read ${header.command}")

        lateinit var payload : ByteArray
        // Process payload
        channels.input.read(header.length) { bytebuffer ->
            Packet.readPayload(bytebuffer, header).onSuccess {
                payload = it
            }.getOrThrow()
        }
        when (header.command) {
            Command.CMD_GENERIC_ACK -> {
                val ack = GenericAck.fromJson(payload.decodeToString())
                if (!ack.success()) {
                    throw GenericAckException(ack)
                }
            }
            Command.CMD_TRANSFER_FILE -> {
                val jsonDataOffset = payload.indexOfFirst {
                    it == 0xFF.toByte()
                }
                val jsonData = payload.sliceArray(0 until jsonDataOffset)
                val fileData = payload.sliceArray(jsonDataOffset + 1 until payload.size)
                Logging.debug("Payload: ${jsonData.decodeToString()}")
                Logging.debug("File size: ${fileData.size}")
                File(destPath).writeBytes(fileData)
            }
            else -> {
                throw IllegalArgumentException("Invalid response")
            }
        }
        closeSession(channels, sessionToken)
    }
    fun setUploadFileOptions(options: UploadOption) {
        mUploadOption = options
    }

    var destination: InetSocketAddress
        get() = mAddress
        set(value) {
            mAddress = value
        }

    private var mAddress = InetSocketAddress("127.0.0.1", 0)
    private var mUploadOption = UploadOption.MUST_NOT_EXIST

    companion object {
        private var mInstance: SocketContext? = null
        fun getInstance(): SocketContext {
            if (mInstance == null) {
                mInstance = SocketContext()
            }
            return mInstance!!
        }
    }
}