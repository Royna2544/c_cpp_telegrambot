package com.royna.tgbotclient.ui

import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.royna.tgbotclient.SocketCommandNative

abstract class SingleViewModelBase<T, V> : ViewModel() {
    protected var _liveData = MutableLiveData<T>()
    val liveData: LiveData<T> get() = _liveData
    fun setLiveData(inval: T) {
        _liveData.value = inval
    }

    protected val gMainScope = viewModelScope
    abstract suspend fun coroutineFunction(activity: FragmentActivity) : V
    abstract fun execute(activity: FragmentActivity, callback: SocketCommandNative.ICommandCallback)
}