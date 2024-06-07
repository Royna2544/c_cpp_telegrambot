package com.royna.tgbotclient

import android.app.Application
import com.royna.tgbotclient.ui.settings.TgClientSettings

class ClientApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        TgClientSettings.loadConfig(this)
    }
}