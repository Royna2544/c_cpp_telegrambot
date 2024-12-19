package com.royna.tgbotclient.ui.settings

import android.content.Context
import android.os.Bundle
import android.util.Log
import android.widget.Toast
import androidx.preference.EditTextPreference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceManager
import com.royna.tgbotclient.R
import com.royna.tgbotclient.net.SocketContext
import io.ktor.network.sockets.InetSocketAddress
import java.net.UnknownHostException

class TgClientSettings : PreferenceFragmentCompat() {
    private lateinit var mIPAddressEditText: EditTextPreference
    private lateinit var mPortEditText: EditTextPreference

    data class InetConfig(var mHostName: String, var mPort: Int)
    private var mAddress = InetConfig(DEFAULT_INET_ADDRESS, DEFAULT_PORT)

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.preference_main)

        mIPAddressEditText = findPreference("ip_address")!!
        mPortEditText = findPreference("port_number")!!
        PreferenceManager.getDefaultSharedPreferences(requireContext()).apply {
            val mIPAddressStr_in = getString(kIPAddressPref, DEFAULT_INET_ADDRESS)!!
            val mPort_in = getInt(kPortPref, DEFAULT_PORT)

            update(mIPAddressStr_in, mPort_in, true)
        }

        mIPAddressEditText.setOnPreferenceChangeListener { _, newValue ->
            val mNewIPAddressStr = newValue as String
            if (!RegexIPv4.matches(mNewIPAddressStr) and !RegexIPv6.matches(mNewIPAddressStr)) {
                Toast.makeText(context, "Invalid IP address, not saved", Toast.LENGTH_SHORT).show()
                return@setOnPreferenceChangeListener false
            }
            update(mHostName = mNewIPAddressStr)
            true
        }
        mPortEditText.setOnPreferenceChangeListener { _, newValue ->
            val mNewPortStr = newValue as String
            if (mNewPortStr.toIntOrNull() == null) {
                Toast.makeText(context, "Invalid port number, not saved", Toast.LENGTH_SHORT).show()
                return@setOnPreferenceChangeListener false
            }
            update(mPort=mNewPortStr.toInt())
            true
        }
    }

    private fun update(mHostName: String? = null, mPort: Int? = null, force: Boolean = false) {
        var mIPAddressStrChanged = false
        var mPortChanged = false
        if (mHostName?.let { mHostName != mAddress.mHostName || force } == true) {
            mIPAddressStrChanged = true
            mAddress.mHostName = mHostName
        }
        if (mPort?.let { mPort != mAddress.mPort || force } == true) {
            mPortChanged = true
            mAddress.mPort = mPort
        }
        if (mIPAddressStrChanged) {
            mIPAddressEditText.text = mAddress.mHostName
            mIPAddressEditText.summary = mAddress.mHostName
        }
        if (mPortChanged) {
            mPortEditText.summary = mPort.toString()
        }
        if (mIPAddressStrChanged or mPortChanged) {
            if (!updateDestination(mAddress)) {
                Toast.makeText(context, "Cannot apply changes", Toast.LENGTH_SHORT).show()
                return
            }
        }
        PreferenceManager.getDefaultSharedPreferences(requireContext()).apply {
            if (mIPAddressStrChanged) {
                edit().putString(kIPAddressPref, mAddress.mHostName).apply()
            }
            if (mPortChanged) {
                edit().putInt(kPortPref, mAddress.mPort).apply()
            }
        }
    }

    companion object {
        private val RegexIPv4 = Regex("\\b((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\\.){3}(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\\b")
        private val RegexIPv6 = Regex("\\b([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}\\b")
        private const val kIPAddressPref = "ip_address"
        private const val kPortPref = "port_num"
        private const val kTag = "TgClientSettings"

        private const val DEFAULT_INET_ADDRESS = "127.0.0.1"
        private const val DEFAULT_PORT = 50000

        private fun updateDestination(mInetConfig: InetConfig): Boolean {
            try {
                SocketContext.getInstance().destination = InetSocketAddress(mInetConfig.mHostName, mInetConfig.mPort)
            } catch (e: UnknownHostException) {
                Log.e(kTag, "Failed to update destination", e)
                return false
            } catch (e: IllegalArgumentException) {
                Log.e(kTag, "Failed to update destination", e)
                return false
            }
            return true
        }

        fun loadConfig(c: Context) {
            PreferenceManager.getDefaultSharedPreferences(c).apply {
                val mIPAddressStr_in = getString(kIPAddressPref, DEFAULT_INET_ADDRESS)!!
                val mPort_in = getInt(kPortPref, DEFAULT_PORT)
                updateDestination(InetConfig(mIPAddressStr_in, mPort_in))
            }
        }
    }
}