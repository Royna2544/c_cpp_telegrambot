package com.royna.tgbotclient.util

import android.util.Log
import com.royna.tgbotclient.BuildConfig

object Logging {
    private const val TAG = "TGBotCli"
    private val DEBUG = BuildConfig.DEBUG

    fun info(message: String) {
        Log.i(getTag(), message)
    }

    fun error(message: String) {
        Log.e(getTag(), message)
    }

    fun error(message: String, t: Throwable) {
        Log.e(getTag(), message, t)
    }

    fun warn(message: String) {
        Log.w(getTag(), message)
    }

    fun debug(message: String) {
        if (DEBUG) {
            Log.d(getTag(), message)
        }
    }

    fun verbose(message: String) {
        if (DEBUG) {
            Log.v(TAG, message)
        }
    }

    private fun getCaller(): String {
        val myPackageName = BuildConfig.APPLICATION_ID.removeSuffix(".dev")
        val element = Thread.currentThread().stackTrace.find {
            it.className.contains(myPackageName) && !it.className
                .contains(Logging.javaClass.simpleName)
        }
        return element?.className?.substringAfterLast('.')?.substringBefore('$') ?: "Unknown"
    }
    private fun getTag() : String {
        return "$TAG::${getCaller()}"
    }
}
