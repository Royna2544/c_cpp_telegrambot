package com.royna.tgbotclient.net.crypto

import com.royna.tgbotclient.net.data.Packet
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

object AES {
    private const val GCM_TAG_LENGTH = Packet.TAG_LENGTH * 8 // Tag length in bits
    private const val ALGORITHM = "AES/GCM/NoPadding"
    private const val GCM_IV_LENGTH = Packet.INIT_VECTOR_LENGTH
    private val cipher = Cipher.getInstance(ALGORITHM)

    fun encrypt(secretKey: SecretKey, iv: ByteArray, payload: ByteArray) : ByteArray {
        val gcmSpec = GCMParameterSpec(GCM_TAG_LENGTH, iv)
        cipher.init(Cipher.ENCRYPT_MODE, secretKey, gcmSpec)
        return cipher.doFinal(payload)
    }

    fun decrypt(secretKey: SecretKey, iv: ByteArray, payload: ByteArray) : ByteArray {
        val gcmSpec = GCMParameterSpec(GCM_TAG_LENGTH, iv)
        cipher.init(Cipher.DECRYPT_MODE, secretKey, gcmSpec)
        return cipher.doFinal(payload)
    }

    // Generate a random IV
    fun generateIV(): ByteArray {
        val iv = ByteArray(GCM_IV_LENGTH)
        SecureRandom().nextBytes(iv)
        return iv
    }
}