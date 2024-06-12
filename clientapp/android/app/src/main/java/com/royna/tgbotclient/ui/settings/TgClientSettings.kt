package com.royna.tgbotclient.ui.settings

import android.content.Context
import android.os.Bundle
import android.util.Log
import android.widget.Toast
import androidx.preference.CheckBoxPreference
import androidx.preference.EditTextPreference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceManager
import com.royna.tgbotclient.R
import com.royna.tgbotclient.SocketCommandNative

class TgClientSettings : PreferenceFragmentCompat() {
    private lateinit var mIPAddressEditText: EditTextPreference
    private lateinit var mPortEditText: EditTextPreference
    private lateinit var mIPv4Switch: CheckBoxPreference
    private lateinit var mIPv6Switch: CheckBoxPreference
    private var mConfig = SocketCommandNative.DestinationType.IPv4
    private var mIPAddressStr = "127.0.0.1"
    private var mPort = 0

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.preference_main)

        mIPv4Switch = findPreference("ipv4")!!
        mIPv6Switch = findPreference("ipv6")!!
        mIPAddressEditText = findPreference("ip_address")!!
        mPortEditText = findPreference("port_number")!!
        PreferenceManager.getDefaultSharedPreferences(requireContext()).apply {
            val mIPAddressStr_in = getString(kIPAddressPref, mIPAddressStr)!!
            val mConfig_in = toInetConfig(getInt(kInetConfigPref, kInet4))
            val mPort_in = getInt(kPortPref, mPort)

            update(UpdateInfo(mIPAddressStr_in, mConfig_in, mPort_in), true)
        }

        mIPv4Switch.setOnPreferenceClickListener {
            update(UpdateInfo(type = SocketCommandNative.DestinationType.IPv4))
            true
        }
        mIPv6Switch.setOnPreferenceClickListener {
            update(UpdateInfo(type = SocketCommandNative.DestinationType.IPv6))
            true
        }
        mIPAddressEditText.setOnPreferenceChangeListener { _, newValue ->
            val mNewIPAddressStr = newValue as String
            if (!RegexIPv4.matches(mNewIPAddressStr) and !RegexIPv6.matches(mNewIPAddressStr)) {
                Toast.makeText(context, "Invalid IP address, not saved", Toast.LENGTH_SHORT).show()
                return@setOnPreferenceChangeListener false
            }
            update(UpdateInfo(ipaddr = mNewIPAddressStr))
            true
        }
        mPortEditText.setOnPreferenceChangeListener { _, newValue ->
            val mNewPortStr = newValue as String
            if (mNewPortStr.toIntOrNull() == null) {
                Toast.makeText(context, "Invalid port number, not saved", Toast.LENGTH_SHORT).show()
                return@setOnPreferenceChangeListener false
            }
            update(UpdateInfo(port=mNewPortStr.toInt()))
            true
        }
    }

    data class UpdateInfo(var ipaddr: String? = null,
                          var type: SocketCommandNative.DestinationType? = null,
                          var port: Int? = null)

    private fun update(updateInfo: UpdateInfo, force: Boolean = false) {
        var mConfigChanged = false
        var mIPAddressStrChanged = false
        var mPortChanged = false
        if (updateInfo.type?.let { updateInfo.type != mConfig || force } == true) {
            mConfigChanged = true
            mConfig = updateInfo.type!!
        }
        if (updateInfo.ipaddr?.let { updateInfo.ipaddr != mIPAddressStr || force } == true) {
            mIPAddressStrChanged = true
            mIPAddressStr = updateInfo.ipaddr!!
        }
        if (updateInfo.port?.let { updateInfo.port != mPort || force } == true) {
            mPortChanged = true
            mPort = updateInfo.port!!
        }
        if (mConfigChanged) {
            mIPv4Switch.isChecked = mConfig == SocketCommandNative.DestinationType.IPv4
            mIPv6Switch.isChecked = mConfig == SocketCommandNative.DestinationType.IPv6
        }
        if (mIPAddressStrChanged) {
            mIPAddressEditText.text = mIPAddressStr
            mIPAddressEditText.summary = mIPAddressStr
        }
        if (mPortChanged) {
            mPortEditText.summary = mPort.toString()
        }
        PreferenceManager.getDefaultSharedPreferences(requireContext()).apply {
            if (mIPAddressStrChanged) {
                edit().putString(kIPAddressPref, mIPAddressStr).apply()
            }
            if (mConfigChanged) {
                when (mConfig) {
                    SocketCommandNative.DestinationType.IPv4 -> {
                        edit().putInt(kInetConfigPref, kInet4).apply()
                    }
                    SocketCommandNative.DestinationType.IPv6 -> {
                        edit().putInt(kInetConfigPref, kInet6).apply()
                    }
                }
            }
            if (mPortChanged) {
                edit().putInt(kPortPref, mPort).apply()
            }
        }
        if (mConfigChanged or mIPAddressStrChanged or mPortChanged) {
            SocketCommandNative.changeDestinationInfo(mIPAddressStr, mConfig, mPort)
        }
    }
    companion object {
        private val RegexIPv4 = Regex("\\b((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\\.){3}(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\\b")
        private val RegexIPv6 = Regex("\\b([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}\\b")
        private const val kIPAddressPref = "ip_address"
        private const val kInetConfigPref = "inet_version"
        private const val kPortPref = "port_num"
        private const val kInet4 = 4
        private const val kInet6 = 6
        private const val kTag = "TgClientSettings"

        fun toInetConfig(num : Int) : SocketCommandNative.DestinationType {
            return when (num) {
                kInet4 -> {
                    SocketCommandNative.DestinationType.IPv4
                }
                kInet6 -> {
                    SocketCommandNative.DestinationType.IPv6
                }
                else -> {
                    Log.e(kTag, "Invalid inet version, default to IPv4")
                    SocketCommandNative.DestinationType.IPv4
                }
            }
        }
        fun loadConfig(c: Context) {
            PreferenceManager.getDefaultSharedPreferences(c).apply {
                val mIPAddressStr_in = getString(kIPAddressPref, "127.0.0.1")!!
                val mConfig_in =
                    toInetConfig(getInt(kInetConfigPref, kInet4))
                val mPort_in = getInt(kPortPref, 0)
                SocketCommandNative.changeDestinationInfo(mIPAddressStr_in, mConfig_in, mPort_in)
            }
        }
    }
}