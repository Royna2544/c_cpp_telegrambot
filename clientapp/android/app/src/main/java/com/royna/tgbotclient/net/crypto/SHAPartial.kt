package com.royna.tgbotclient.net.crypto

import java.security.MessageDigest

class SHAPartial {
    private val sha256 = MessageDigest.getInstance("SHA-256")

    data class SHAResult(val hash: ByteArray) {
        override fun toString(): String {
            // Convert the byte array to a hexadecimal string
            val hexString = StringBuilder()
            for (b in hash) {
                // Mask to ensure non-negative values
                val hex = Integer.toHexString(0xff and b.toInt())
                if (hex.length == 1) {
                    hexString.append('0') // Add leading zero if needed
                }
                hexString.append(hex)
            }
            return hexString.toString()
        }

        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false

            other as SHAResult

            return hash.contentEquals(other.hash)
        }

        override fun hashCode(): Int {
            return hash.contentHashCode()
        }
    }

    fun create(payload: ByteArray): SHAResult {
        sha256.update(payload)
        return SHAResult(sha256.digest())
    }
}