package com.royna.tgbotclient.util

import android.app.Activity
import androidx.window.layout.WindowMetricsCalculator

object DeviceUtils {
    fun getScreenHeight(activity: Activity): Int = WindowMetricsCalculator.getOrCreate()
        .computeCurrentWindowMetrics(activity).bounds.height()

    fun getScreenWidth(activity: Activity): Int = WindowMetricsCalculator.getOrCreate()
        .computeCurrentWindowMetrics(activity).bounds.width()
}