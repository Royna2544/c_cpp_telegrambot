package com.royna.tgbotclient.pm

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatActivity
import com.royna.tgbotclient.BuildConfig

@RequiresApi(Build.VERSION_CODES.R)
class StoragePermissionR : IStoragePermission {
    override fun init(activity: AppCompatActivity, callback: IStoragePermission.IRequestResult) {
        kActivityRequest = activity.registerForActivityResult(ActivityResultContracts
            .StartActivityForResult()) {
            if (Environment.isExternalStorageManager()) {
                callback.onGranted()
            } else {
                callback.onDenied()
            }
        }
    }

    override fun isGranted(activity: AppCompatActivity): Boolean {
        return Environment.isExternalStorageManager()
    }

    override fun request() {
        kActivityRequest.launch(Intent(
            Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
            Uri.parse("package:${BuildConfig.APPLICATION_ID}")
        ))
    }
    lateinit var kActivityRequest: ActivityResultLauncher<Intent>
}