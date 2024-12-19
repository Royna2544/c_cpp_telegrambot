package com.royna.tgbotclient.util

import android.app.Activity
import android.content.Context
import android.view.View
import android.view.inputmethod.InputMethodManager
import androidx.window.layout.WindowMetricsCalculator

object DeviceUtils {
    fun getScreenHeight(activity: Activity): Int = WindowMetricsCalculator.getOrCreate()
        .computeCurrentWindowMetrics(activity).bounds.height()

    fun getScreenWidth(activity: Activity): Int = WindowMetricsCalculator.getOrCreate()
        .computeCurrentWindowMetrics(activity).bounds.width()

    fun hideKeyboard(view: View) {
        val imm = view.context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        imm.hideSoftInputFromWindow(view.windowToken, 0)
    }
}