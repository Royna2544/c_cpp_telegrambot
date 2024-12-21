package com.royna.tgbotclient.pm

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity

class StoragePermissionPreR : IStoragePermission {
    override fun init(activity: AppCompatActivity, callback: IStoragePermission.IRequestResult)  {
        kActivityRequest = activity.registerForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()) {
            if (isGranted(activity)) {
                callback.onGranted()
            } else {
                callback.onDenied()
            }
        }
    }

    override fun request() {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            kActivityRequest.launch(kPermissionsRequested.toTypedArray())
        }
    }

    override fun isGranted(activity: AppCompatActivity): Boolean {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            return kPermissionsRequested.all { permission ->
                activity.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED
            }
        }
        return true
    }

    private lateinit var kActivityRequest : ActivityResultLauncher<Array<String>>
    private val kPermissionsRequested: List<String>
        get() = listOf(
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE)

}