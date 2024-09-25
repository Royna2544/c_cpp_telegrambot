package com.royna.tgbotclient.ui.commands.uploadfile

import android.net.Uri
import androidx.fragment.app.FragmentActivity
import com.royna.tgbotclient.SocketCommandNative
import com.royna.tgbotclient.ui.SingleViewModelBase
import com.royna.tgbotclient.util.FileUtils.copyToExt
import com.royna.tgbotclient.util.FileUtils.getFileExtension
import com.royna.tgbotclient.util.FileUtils.queryFileName
import com.royna.tgbotclient.util.Logging
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class UploadFileViewModel : SingleViewModelBase<Uri, Unit>() {
    override suspend fun coroutineFunction(activity: FragmentActivity) = suspendCancellableCoroutine {
        cancellableContinuation ->
        val contentUri = liveData.value!!
        val fileExtension = getFileExtension(activity, contentUri)
        val fileName = "temp_file" + if (fileExtension != null) ".$fileExtension" else ""

        Logging.info("Creating temporary file: $fileName")

        // Creating Temp file
        val tempFile = File(activity.cacheDir, fileName)
        tempFile.createNewFile()
        tempFile.deleteOnExit()

        try {
            activity.contentResolver.openInputStream(contentUri)?.copyToExt(FileOutputStream(tempFile))
        } catch (e: Exception) {
            e.printStackTrace()
            cancellableContinuation.resumeWithException(e)
            return@suspendCancellableCoroutine
        }

        val name = queryFileName(activity.contentResolver,contentUri) ?: fileName
        Logging.info("Uploading file as : $name")
        SocketCommandNative.uploadFile(tempFile.absolutePath, name,
            object : SocketCommandNative.ICommandCallback {
            override fun onSuccess(result: Any?) {
                Logging.info ("File uploaded successfully")
                cancellableContinuation.resume(Unit)
            }

            override fun onError(error: String) {
                Logging.error ("File upload failed: $error")
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