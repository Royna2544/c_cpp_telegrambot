package com.royna.tgbotclient.pm

import androidx.appcompat.app.AppCompatActivity

interface IStoragePermission {
    interface IRequestResult {
        fun onGranted()
        fun onDenied()
    }
    fun init(activity: AppCompatActivity, callback: IRequestResult)
    fun request()
    fun isGranted(activity: AppCompatActivity) : Boolean
}