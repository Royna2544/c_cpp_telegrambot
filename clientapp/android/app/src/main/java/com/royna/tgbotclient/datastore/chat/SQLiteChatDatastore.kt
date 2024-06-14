package com.royna.tgbotclient.datastore.chat

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import com.royna.tgbotclient.datastore.IDataStore
import com.royna.tgbotclient.util.Logging

class SQLiteChatDatastore(context: Context) : IDataStore<String, ChatId>,
    SQLiteOpenHelper(context, DATABASE_NAME, null, DATABASE_VERSION) {
    override fun create(): Boolean = true
    override fun readAll(): Map<String, ChatId> {
        Logging.verbose("Reading all")
        val result = mutableMapOf<String, ChatId>()
        readableDatabase.use { db ->
            db.query(
                TABLE_NAME,
                arrayOf(COLUMN_NAME, COLUMN_ID),
                null,
                null,
                null,
                null,
                null
            ).use {
                while (it.moveToNext()) {
                    result[it.getString(0)] = it.getLong(1)
                }
            }
            Logging.verbose("Read all: result $result")
        }
        return result
    }

    override fun delete(key: String): Boolean {
        val success : Boolean

        Logging.verbose("Deleting $key")
        readableDatabase.use {
            success = it.delete(TABLE_NAME, "$COLUMN_NAME = ?", arrayOf(key)) == 1
        }
        Logging.verbose("Deleted $key: result $success")
        return success
    }

    override fun write(key: String, value: ChatId): Boolean {
        Logging.verbose("Writing $key:$value")
        writableDatabase.use { wdb ->
            val cntv = ContentValues().apply {
                put(COLUMN_NAME, key)
                put(COLUMN_ID, value)
            }
            return runCatching {
                wdb.insertOrThrow(TABLE_NAME, null, cntv) > 0
            }.getOrElse {
                Logging.error("Failed to write $key:$value", it)
                false
            }
        }
    }

    override fun read(key: String): ChatId? {
        var chatId : ChatId? = null

        Logging.verbose("Reading $key")
        readableDatabase.use { db ->
            db.query(
                TABLE_NAME,
                arrayOf(COLUMN_ID),
                "$COLUMN_NAME = ?",
                arrayOf(key),
                null,
                null,
                null
            ).use {
                if (it.moveToFirst()) {
                    chatId = it.getLong(0)
                }
            }
        }
        Logging.verbose("Read $key: result $chatId")
        return chatId
    }

    override fun clearAll(): Boolean {
        Logging.verbose("Clearing all")
        val success = writableDatabase.delete(TABLE_NAME, null, null) > 0
        Logging.verbose("Cleared all: result $success")
        return success
    }

    override fun findKey(value: ChatId): String? {
        Logging.verbose("Finding $value")
        readableDatabase.use { db ->
            db.query(
                TABLE_NAME,
                arrayOf(COLUMN_NAME),
                "$COLUMN_ID = ?",
                arrayOf(value.toString()),
                null,
                null,
                null).use {
                if (it.moveToFirst()) {
                    return it.getString(0)
                }
            }
        }
        return null
    }

    override fun onCreate(db: SQLiteDatabase?) {
        db?.execSQL("CREATE TABLE $TABLE_NAME ($COLUMN_ID BIGINT UNIQUE," +
                "$COLUMN_NAME TEXT UNIQUE NOT NULL)")
    }

    override fun onUpgrade(db: SQLiteDatabase?, oldVersion: Int, newVersion: Int) {
        if (oldVersion < newVersion) {
            db?.execSQL("DROP TABLE IF EXISTS $TABLE_NAME")
            onCreate(db)
        }
    }

    companion object {
        const val DATABASE_NAME = "chat_ids.db"
        const val DATABASE_VERSION = 1
        const val TABLE_NAME = "chat_ids"
        const val COLUMN_ID = "id"
        const val COLUMN_NAME = "name"
    }
}

typealias ChatId = Long