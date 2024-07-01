package com.royna.tgbotclient

import android.app.Application
import com.royna.tgbotclient.ui.settings.TgClientSettings
import com.royna.tgbotclient.util.Logging
import dagger.hilt.android.HiltAndroidApp

@HiltAndroidApp
class ClientApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        TgClientSettings.loadConfig(this)
        Logging.info("Application created")
    }
}