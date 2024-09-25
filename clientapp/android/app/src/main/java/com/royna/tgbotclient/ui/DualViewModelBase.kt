package com.royna.tgbotclient.ui

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData

abstract class DualViewModelBase<T, V, U> : SingleViewModelBase<T, U>() {
    private var _liveData2 = MutableLiveData<V>()
    val liveData2: LiveData<V> get() = _liveData2
    fun setLiveData2(inval: V) {
        _liveData2.value = inval
    }
}