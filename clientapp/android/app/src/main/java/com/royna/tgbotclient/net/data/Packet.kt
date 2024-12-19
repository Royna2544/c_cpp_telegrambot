package com.royna.tgbotclient.net.data

import com.royna.tgbotclient.net.crypto.AES
import com.royna.tgbotclient.net.crypto.HMAC
import com.royna.tgbotclient.util.Logging
import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.crypto.spec.SecretKeySpec

data class Packet(val command: Command,
                  val payloadType: PayloadType,
                  val length: Int,
                  val sessionToken : String,
                  val hmac : ByteArray,
                  val initVector: ByteArray,
                  var payload: ByteArray? = null)  {
    companion object {
        const val MAGIC: Long = 0xDEADFACE
        const val HEADER_VERSION: Int = 12
        const val HMAC_LENGTH = 64
        const val PACKET_HEADER_LEN = 144

        // Specification of AES-GCM
        const val SESSION_TOKEN_LENGTH = 32
        const val INIT_VECTOR_LENGTH = 12
        const val TAG_LENGTH = 16

        fun create(command: Command, payloadType: PayloadType, sessionToken: String, payload: ByteArray? = null) = runCatching {
            val initVector = AES.generateIV()
            val hmac = ByteArray(HMAC_LENGTH)
            var data = payload

            if (data != null) {
                data = AES.encrypt(SecretKeySpec(sessionToken.toByteArray(), "AES"), initVector, data)
                HMAC.compute(sessionToken.toByteArray(), data).copyInto(hmac)
            }

            val header = Packet(
                command = command,
                payloadType = payloadType,
                hmac = hmac,
                length = data?.size ?: 0,
                sessionToken = sessionToken,
                initVector = initVector,
                payload = data
            )
            header.toByteArray().getOrThrow()
        }

        private inline fun <reified E : Enum<E>> ByteBuffer.getEnumByValue(
            crossinline valueSelector: (E) -> Int
        ): E {
            val intValue = this.getInt()
            return enumValues<E>().find { valueSelector(it) == intValue }
                ?: throw IllegalArgumentException("Invalid enum ${E::class.simpleName} value: $intValue")
        }

        fun fromByteBuffer(buffer: ByteBuffer) = runCatching {
            // We always use little endian
            buffer.order(ByteOrder.LITTLE_ENDIAN)

            val magic = buffer.getLong()
            require(magic == MAGIC + HEADER_VERSION) { "Invalid magic number" }

            val command : Command = buffer.getEnumByValue { it.value }
            val payloadType: PayloadType = buffer.getEnumByValue { it.value }
            val length = buffer.getInt()
            val sessionToken = ByteArray(SESSION_TOKEN_LENGTH)
            buffer.get(sessionToken)
            buffer.getInt()  // padding 1
            buffer.getLong() // nonce
            val macBuffer = ByteArray(HMAC_LENGTH)
            buffer.get(macBuffer)
            val initVectorBuffer = ByteArray(INIT_VECTOR_LENGTH)
            buffer.get(initVectorBuffer)
            buffer.getInt() // padding 2
            Logging.debug("Read header: version: $HEADER_VERSION command: $command payloadType: $payloadType length: $length")

            Packet(
                command, payloadType, length, sessionToken.decodeToString(), macBuffer, initVectorBuffer, ByteArray(0)
            )
        }
        fun readPayload(buffer: ByteBuffer, packet: Packet) = runCatching {
            // Fetch Payload
            val payload = ByteArray(packet.length)
            buffer.get(payload)

            // Match HMAC
            val computedHMAC = HMAC.compare(packet.sessionToken.toByteArray(), payload, packet.hmac)
            require(computedHMAC) { "Invalid HMAC" }

            // Decrypt
            AES.decrypt(SecretKeySpec(packet.sessionToken.toByteArray(),
                "AES"), packet.initVector, payload)
        }
    }

    fun toByteArray() = runCatching {
        // Validate hmac and initVector lengths
        require(hmac.size == HMAC_LENGTH) { "HMAC must be $HMAC_LENGTH bytes" }
        require(initVector.size == INIT_VECTOR_LENGTH) { "Initialization Vector must be $INIT_VECTOR_LENGTH bytes" }
        require(sessionToken.length == SESSION_TOKEN_LENGTH) { "Session Token must be $SESSION_TOKEN_LENGTH bytes" }

        // Create a ByteBuffer to write the header
        val buffer = ByteBuffer.allocate(PACKET_HEADER_LEN + (payload?.size ?: 0))
        buffer.order(ByteOrder.LITTLE_ENDIAN)

        Logging.debug("Write header: version: $HEADER_VERSION command: $command payloadType: $payloadType length: $length")

        // Write fields to buffer
        buffer.putLong(MAGIC + HEADER_VERSION)  // magic
        buffer.putInt(command.value)                  // command
        buffer.putInt(payloadType.value)              // payload type
        buffer.putInt(length)                         // length
        buffer.put(sessionToken.toByteArray())        // session token
        buffer.putInt(0)                        // padding 1
        buffer.putLong(System.currentTimeMillis())    // nonce
        buffer.put(hmac)                              // HMAC
        buffer.put(initVector)                        // Initialization Vector
        buffer.putInt(0)                        // padding 2

        require(buffer.position() == PACKET_HEADER_LEN) {
            "Buffer size mismatch: ${buffer.position()} and $PACKET_HEADER_LEN"
        }

        payload?.let { buffer.put(it) }                // payload
        buffer.array()
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as Packet

        if (command != other.command) return false
        if (payloadType != other.payloadType) return false
        if (length != other.length) return false
        if (!sessionToken.contentEquals(other.sessionToken)) return false
        if (!hmac.contentEquals(other.hmac)) return false
        if (!initVector.contentEquals(other.initVector)) return false
        if (!payload.contentEquals(other.payload)) return false

        return true
    }

    override fun hashCode(): Int {
        var result = command.hashCode()
        result = 31 * result + payloadType.hashCode()
        result = 31 * result + length
        result = 31 * result + sessionToken.hashCode()
        result = 31 * result + hmac.contentHashCode()
        result = 31 * result + initVector.contentHashCode()
        result = 31 * result + payload.contentHashCode()
        return result
    }

}