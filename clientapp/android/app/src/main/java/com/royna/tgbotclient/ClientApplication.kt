package com.royna.tgbotclient

import android.app.Application
import com.royna.tgbotclient.ui.settings.TgClientSettings
import com.royna.tgbotclient.util.Logging
import io.github.oshai.kotlinlogging.KLogger
import io.github.oshai.kotlinlogging.KotlinLogging

class ClientApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        TgClientSettings.loadConfig(this)
        System.setProperty("kotlin-logging-to-android-native", "true")
        Logging.i { "Application created" }
    }
}