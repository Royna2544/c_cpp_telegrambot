package com.royna.tgbotclient.ui.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.royna.tgbotclient.R

class SettingsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.settings_activity)
        if (savedInstanceState == null) {
            supportFragmentManager
                .beginTransaction()
                .replace(R.id.settings, TgClientSettings())
                .commit()
        }
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
    }
}