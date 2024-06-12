package com.royna.tgbotclient.ui.commands.uploadfile

import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.FileUtils
import android.webkit.MimeTypeMap
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.royna.tgbotclient.SocketCommandNative
import com.royna.tgbotclient.ui.commands.SingleViewModelBase
import com.royna.tgbotclient.util.Logging
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class UploadFileViewModel : SingleViewModelBase<String, Unit>() {
    private fun getFileExtension(context: Context, uri: Uri): String? {
        val fileType: String? = context.contentResolver.getType(uri)
        return MimeTypeMap.getSingleton().getExtensionFromMimeType(fileType)
    }

    @Throws(IOException::class)
    private fun copyFileStreams(source: InputStream, target: OutputStream) {
        val buf = ByteArray(8192)
        var length: Int
        while (source.read(buf).also { length = it } > 0) {
            target.write(buf, 0, length)
        }
    }

    override suspend fun coroutineFunction(activity: FragmentActivity) = suspendCancellableCoroutine {
        cancellableContinuation ->
        val contentUri = Uri.parse(liveData.value)
        val fileExtension = getFileExtension(activity, contentUri)
        val fileName = "temp_file" + if (fileExtension != null) ".$fileExtension" else ""

        Logging.i { "Creating temporary file: $fileName" }

        // Creating Temp file
        val tempFile = File(activity.cacheDir, fileName)
        tempFile.createNewFile()

        try {
            val oStream = FileOutputStream(tempFile)
            activity.contentResolver.openInputStream(contentUri)?.let { stream ->
                stream.use {
                    if (Build.VERSION.SDK_INT > Build.VERSION_CODES.P) {
                        FileUtils.copy(it, oStream)
                    } else {
                        copyFileStreams(it, oStream)
                    }
                }
            }
            oStream.flush()
        } catch (e: Exception) {
            e.printStackTrace()
            cancellableContinuation.resumeWithException(e)
            return@suspendCancellableCoroutine
        }
        SocketCommandNative.uploadFile(tempFile.absolutePath, fileName,
            object : SocketCommandNative.ICommandCallback {
            override fun onSuccess(result: Any?) {
                Logging.i { "File uploaded successfully" }
                cancellableContinuation.resume(Unit)
            }

            override fun onError(error: String) {
                Logging.e { "File upload failed: $error" }
                cancellableContinuation.resumeWithException(RuntimeException(error))
            }
        })
    }
    override fun execute(activity: FragmentActivity, callback: SocketCommandNative.ICommandCallback) {
        gMainScope.launch {
            try {
                withContext(Dispatchers.IO){
                    coroutineFunction(activity)
                }
                callback.onSuccess(null)
            } catch (e: RuntimeException) {
                callback.onError(e.message.toString())
            }
        }
    }

}