package com.royna.tgbotclient.util

import android.util.Log
import com.royna.tgbotclient.BuildConfig

object Logging {
    private const val TAG = "TGBotCli::APP"
    private val DEBUG = BuildConfig.DEBUG

    fun info(message: String) {
        Log.i(TAG, "[${getCaller()}] $message")
    }

    fun error(message: String) {
        Log.e(TAG, "[${getCaller()}] $message")
    }
    fun error(message: String, t: Throwable) {
        Log.e(TAG, "[${getCaller()}] $message", t)
    }

    fun warn(message: String) {
        Log.w(TAG, "[${getCaller()}] $message")
    }

    fun debug(message: String) {
        if (DEBUG) {
            Log.d(TAG, "[${getCaller()}] $message")
        }
    }

    fun verbose(message: String) {
        if (DEBUG) {
            Log.v(TAG, "[${getCaller()}] $message")
        }
    }

    private fun getCaller(): String {
        val myPackageName = BuildConfig.APPLICATION_ID.removeSuffix(".dev")
        val element = Thread.currentThread().stackTrace.find {
            it.className.contains(myPackageName) && !it.className
                .contains(Logging.javaClass.simpleName)
        }
        if (element == null) {
            // Raise empty exception to log stack trace
            Log.e(TAG, "getCaller: Failed to find caller", Exception())
            return "Unknown"
        }
        var className = element.className.removePrefix("$myPackageName.")
        val methodName = element.methodName
        if (className.contains("$")) {
            val classNameSplit = className.split("$")
            // Actual class name
            className = classNameSplit[0]
            // Method of the class name
            val memberFunctionName = classNameSplit[1]
            className = "(anonymous class of $className::$memberFunctionName)"
        }
        return "Class '$className', Method '$methodName'"
    }
}
