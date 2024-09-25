package com.royna.tgbotclient.ui.commands.downloadfile

import android.content.Context
import android.net.Uri
import android.webkit.MimeTypeMap
import androidx.documentfile.provider.DocumentFile
import androidx.fragment.app.FragmentActivity
import com.royna.tgbotclient.SocketCommandNative
import com.royna.tgbotclient.ui.DualViewModelBase
import com.royna.tgbotclient.util.FileUtils.copyFromExt
import com.royna.tgbotclient.util.FileUtils.queryFileName
import com.royna.tgbotclient.util.Logging
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class DownloadFileViewModel : DualViewModelBase<Uri, String, DownloadFileViewModel.Cleanup>() {
    inner class Cleanup(private val context: Context,
                        private val tempfile: File,
                        private val newFileUri: Uri) {
        fun delete() {
            tempfile.delete()
            DocumentFile.fromSingleUri(context, newFileUri)?.delete()
        }
    }

    override suspend fun coroutineFunction(activity: FragmentActivity) = suspendCancellableCoroutine {
        cancellableContinuation ->
        val contentUri = liveData.value!!
        val outFile = File(liveData2.value!!)
        val tempFile = File(activity.cacheDir, outFile.name)
        tempFile.delete()
        outFile.delete()

        val mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(outFile.extension) ?: "application/octet-stream"
        val docFile = DocumentFile.fromTreeUri(activity, contentUri)?.run {
            findFile(outFile.name)?.let {
                Logging.info("Deleting existing file")
                it.delete()
            }
            createFile(mimeType, outFile.name)?.uri
        }

        if (docFile == null) {
            cancellableContinuation.resumeWithException(RuntimeException("Could not create file"))
            return@suspendCancellableCoroutine
        }

        val downloadedFilePath = queryFileName(activity.contentResolver, docFile)!!
        Logging.info("Downloading file as : $downloadedFilePath")
        var downloadRet = false
        SocketCommandNative.downloadFile(downloadedFilePath, tempFile.absolutePath,
            object : SocketCommandNative.ICommandStatusCallback {
                override fun onStatusUpdate(status: SocketCommandNative.Status) {
                }

                override fun onSuccess(result: Any?) {
                    Logging.info ("File uploaded successfully")
                    downloadRet = true
                }

                override fun onError(error: String) {
                    Logging.error ("File upload failed: $error")
                    cancellableContinuation.resumeWithException(RuntimeException(error))
                }
            }
        )
        if (!downloadRet) {
            return@suspendCancellableCoroutine
        }

        try {
            activity.contentResolver.openOutputStream(docFile)?.copyFromExt(FileInputStream(tempFile))
        } catch (e: IOException) {
            Logging.error("Failed to copy to destination", e)
            cancellableContinuation.resumeWithException(
                RuntimeException("Failed to copy downloaded file to destination"))
            return@suspendCancellableCoroutine
        }
        cancellableContinuation.resume(Cleanup(activity, tempFile, docFile))
    }

    override fun execute(activity: FragmentActivity, callback: SocketCommandNative.ICommandCallback) {
        gMainScope.launch {
            try {
                withContext(Dispatchers.IO){
                    coroutineFunction(activity).delete()
                }
                callback.onSuccess(null)
            } catch (e: RuntimeException) {
                callback.onError(e.message.toString())
            }
        }
    }
}