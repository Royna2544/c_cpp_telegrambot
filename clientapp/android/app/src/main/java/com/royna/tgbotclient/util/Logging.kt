package com.royna.tgbotclient.util

import com.royna.tgbotclient.BuildConfig
import io.github.oshai.kotlinlogging.KotlinLogging

object Logging {
    private const val TAG = "TGBotCli::APP"
    private val DEBUG = BuildConfig.DEBUG
    fun i(fn: logFunctionT) {
        KotlinLogging.logger(getCaller()).info(fn)
    }

    fun e(fn: logFunctionT) {
        KotlinLogging.logger(getCaller()).error(fn)
    }

    fun w(fn: logFunctionT) {
        KotlinLogging.logger(getCaller()).warn(fn)
    }

    fun d(fn: logFunctionT) {
        if (DEBUG) {
            KotlinLogging.logger(getCaller()).debug(fn)
        }
    }

    private fun getCaller(): String {
        val element = Thread.currentThread().stackTrace[4]
        var className = element.className.replace("$", "_")
        while (className.endsWith("_")) {
            className = className.substring(0, className.length - 1)
        }
        return String.format(
            "%s %s",
            className.substring(className.lastIndexOf(".") + 1),
            element.methodName.substringBefore("-")
        )
    }
}

typealias logFunctionT = () -> Any?