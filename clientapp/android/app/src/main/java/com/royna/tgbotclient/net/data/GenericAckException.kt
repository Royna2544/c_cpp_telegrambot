package com.royna.tgbotclient.net.data

class GenericAckException(val data: GenericAck) : RuntimeException() {
    override val message: String
        get() {
            return "${data.ackType}: ${data.message}"
        }
}