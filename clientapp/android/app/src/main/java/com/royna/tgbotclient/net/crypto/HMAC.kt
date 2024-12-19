package com.royna.tgbotclient.net.crypto

import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

object HMAC {
    private const val ALGORITHM = "HmacSHA256"
    private val hmac = Mac.getInstance(ALGORITHM)

    fun compute(secretKey: ByteArray, payload: ByteArray): ByteArray {
        hmac.init(SecretKeySpec(secretKey, ALGORITHM))
        hmac.update(payload)
        return hmac.doFinal()
    }

    fun compare(secretKey: ByteArray, payload: ByteArray, expectedHMAC: ByteArray): Boolean {
        return compute(secretKey, payload).copyOf(expectedHMAC.size).contentEquals(expectedHMAC)
    }
}