package com.royna.tgbotclient.ui.commands.uploadfile

import android.net.Uri
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.lifecycleScope
import com.royna.tgbotclient.net.SocketContext
import com.royna.tgbotclient.util.FileUtils.copyToExt
import com.royna.tgbotclient.util.FileUtils.getFileExtension
import com.royna.tgbotclient.util.FileUtils.queryFileName
import com.royna.tgbotclient.util.Logging
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream

class UploadFileViewModel : ViewModel() {
    // The Uri of the selected file
    private val sourceFileUri = MutableLiveData<Uri>()
    fun setFileUri(uri: Uri) {
        sourceFileUri.value = uri
    }

    // The result of the upload
    private val _uploadResult = MutableSharedFlow<String>()
    val uploadResult: SharedFlow<String>
        get() = _uploadResult


    private suspend fun uploadFile(activity: FragmentActivity) = runCatching {
        val contentUri = sourceFileUri.value!!
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
            Logging.error("Failed to copy to destination", e)
            throw e
        }

        val name = queryFileName(activity.contentResolver,contentUri) ?: fileName
        Logging.info("Uploading file as : $name")
        SocketContext.getInstance().uploadFile(tempFile, name).getOrThrow()
    }

    fun execute(activity: FragmentActivity) {
        activity.lifecycleScope.launch {
            withContext(Dispatchers.IO){
                uploadFile(activity)
            }.onFailure {
                Logging.error("Failed to upload file", it)
                _uploadResult.emit("Failed to upload file: ${it.message}")
            }.onSuccess {
                Logging.info("File uploaded")
                _uploadResult.emit("File uploaded")
            }
        }
    }
}