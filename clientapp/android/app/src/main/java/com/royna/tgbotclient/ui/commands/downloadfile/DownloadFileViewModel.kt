package com.royna.tgbotclient.ui.commands.downloadfile

import android.net.Uri
import android.webkit.MimeTypeMap
import androidx.documentfile.provider.DocumentFile
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.lifecycleScope
import com.royna.tgbotclient.net.SocketContext
import com.royna.tgbotclient.util.FileUtils.copyFromExt
import com.royna.tgbotclient.util.FileUtils.queryFileName
import com.royna.tgbotclient.util.Logging
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.io.IOException

class DownloadFileViewModel : ViewModel() {
    // The URI to write the resulting file
    private var _outFileUri = MutableLiveData<Uri>()
    // Source path in the server
    private val _sourceFilePath = MutableLiveData<String>()
    // Event when a download succeeded
    private val _downloadEvent = MutableSharedFlow<String>()

    val downloadEvent: SharedFlow<String>
        get() = _downloadEvent
    val outFileURI : LiveData<Uri>
        get() = _outFileUri
    val sourceFilePath : LiveData<String>
        get() = _sourceFilePath

    fun setFileUrl(uri: Uri) {
        _outFileUri.value = uri
    }
    fun setSourceFile(path: String) {
        _sourceFilePath.value = path
    }

    private fun openFile(activity: FragmentActivity): Uri {
        val contentUri = _outFileUri.value!!
        val outFile = File(_sourceFilePath.value!!)
        val mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(outFile.extension) ?: "application/octet-stream"
        val docFile = DocumentFile.fromTreeUri(activity, contentUri)?.run {
            findFile(outFile.name)?.let {
                Logging.info("Deleting existing file")
                it.delete()
            }
            createFile(mimeType, outFile.name)?.uri
        }

        if (docFile == null) {
            throw RuntimeException("Could not create file in the selected directory. Ensure you have write permissions.")
        }
        return docFile
    }

    private suspend fun downloadFile(activity: FragmentActivity) = runCatching {
        val docFile = openFile(activity)
        val tempFile = File(activity.cacheDir, docFile.lastPathSegment ?: "tmp.bin")
        tempFile.delete()

        val downloadedFilePath = queryFileName(activity.contentResolver, docFile)!!
        Logging.info("Downloading file as : $downloadedFilePath")

        SocketContext.getInstance().downloadFile(downloadedFilePath, tempFile.absolutePath).getOrElse {
            Logging.error("Failed to download file", it)
            tempFile.delete()
            DocumentFile.fromSingleUri(activity, docFile)?.delete()
            throw it
        }

        try {
            activity.contentResolver.openOutputStream(docFile)?.copyFromExt(FileInputStream(tempFile))
        } catch (e: IOException) {
            Logging.error("Failed to copy to destination", e)
            throw e
        } finally {
            tempFile.delete()
            DocumentFile.fromSingleUri(activity, docFile)?.delete()
        }
    }

    fun execute(activity: FragmentActivity) {
        activity.lifecycleScope.launch {
            withContext(Dispatchers.IO) {
                downloadFile(activity)
            }.onSuccess {
                Logging.info("File downloaded")
                _downloadEvent.emit("File downloaded")
            }.onFailure {
                Logging.error("Failed to download file", it)
                _downloadEvent.emit("Failed to download file: ${it.message}")
            }
        }
    }
}