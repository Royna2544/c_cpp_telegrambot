package com.royna.tgbotclient.ui.commands.uploadfile

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.FileUtils
import android.provider.OpenableColumns
import android.webkit.MimeTypeMap
import androidx.fragment.app.FragmentActivity
import com.royna.tgbotclient.SocketCommandNative
import com.royna.tgbotclient.ui.SingleViewModelBase
import com.royna.tgbotclient.util.Logging
import kotlinx.coroutines.Dispatchers
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

class UploadFileViewModel : SingleViewModelBase<Uri, Unit>() {
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

    fun queryName(resolver: ContentResolver, uri: Uri): String? {
        val returnCursor =
            resolver.query(uri, null, null, null, null) ?: return null
        val nameIndex = returnCursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
        returnCursor.moveToFirst()
        val name = returnCursor.getString(nameIndex)
        returnCursor.close()
        return name
    }

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

        val name = queryName(activity.contentResolver,contentUri) ?: fileName
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