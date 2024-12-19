package com.royna.tgbotclient.net.data

import com.google.gson.Gson

data class GenericAck(
    val ackType: AckType,
    val message: String
) {
    enum class AckType {
        SUCCESS,
        ERROR_TGAPI_EXCEPTION,
        ERROR_INVALID_ARGUMENT,
        ERROR_COMMAND_IGNORED,
        ERROR_RUNTIME_ERROR,
        ERROR_CLIENT_ERROR,
    }

    data class Json(val result: Boolean, val error_type: String, val error_msg: String)

    companion object {
        fun fromJson(json: String): GenericAck {
            val gson = Gson()
            val jsonSerialized = gson.fromJson(json, Json::class.java)

            if (jsonSerialized.result) {
                return GenericAck(
                    ackType = AckType.SUCCESS,
                    message = ""
                )
            }
            for (type in AckType.entries) {
                if (type.name.endsWith(jsonSerialized.error_type)) {
                    return GenericAck(
                        ackType = type,
                        message = jsonSerialized.error_msg
                    )
                }
            }
            throw IllegalArgumentException("Unknown error type: ${jsonSerialized.error_type}")
        }
    }

    fun success() = ackType == AckType.SUCCESS
}