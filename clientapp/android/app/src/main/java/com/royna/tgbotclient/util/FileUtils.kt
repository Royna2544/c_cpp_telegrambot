package com.royna.tgbotclient.util

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.FileUtils
import android.provider.OpenableColumns
import android.webkit.MimeTypeMap
import java.io.Closeable
import java.io.File
import java.io.InputStream
import java.io.OutputStream

object FileUtils {
    fun getFileExtension(context: Context, uri: Uri): String? {
        val fileType: String? = context.contentResolver.getType(uri)
        return MimeTypeMap.getSingleton().getExtensionFromMimeType(fileType)
    }

    fun queryFileName(resolver: ContentResolver, uri: Uri): String? {
        val returnCursor =
            resolver.query(uri, null, null, null, null) ?: return null
        val nameIndex = returnCursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
        returnCursor.moveToFirst()
        val name = returnCursor.getString(nameIndex)
        returnCursor.close()
        return name
    }

    fun List<String>.dirJoin() : String {
        val sb = StringBuilder()
        for (x in withIndex()) {
            sb.append(x.value)
            if (x.index != size - 1) {
                sb.append(File.separatorChar)
            }
        }
        return sb.toString()
    }

    private fun <T : Closeable, V : Closeable> useBoth(closeable1: T, closeable2: V, block: (T, V) -> Unit) {
        closeable1.use {
            closeable2.use {
                block(closeable1, closeable2)
            }
        }
    }

    private fun copy(source: InputStream, target: OutputStream) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.P) {
            FileUtils.copy(source, target)
        } else {
            useBoth(source, target) { istream, ostream ->
                istream.copyTo(ostream)
            }
        }
    }

    // Ext of copyFrom in Kotlin stdlib, with the suffix to discriminate from that one
    fun OutputStream.copyFromExt(inputStream: InputStream) = copy(inputStream, this)
    // Ext of copyTo in Kotlin stdlib, with the suffix to discriminate from that one
    fun InputStream.copyToExt(outputStream: OutputStream) = copy(this, outputStream)
}
