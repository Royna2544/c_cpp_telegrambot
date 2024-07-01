package com.royna.tgbotclient.datastore

import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query

@Dao
interface IChatIDOperations {
    @Insert
    fun add(vararg entries: ChatIDEntry)

    @Query("SELECT * FROM ChatIDEntry")
    fun getAll(): List<ChatIDEntry>

    @Delete
    fun remove(entry: ChatIDEntry)

    @Query("SELECT chat_id FROM ChatIDEntry WHERE chat_name = :name")
    fun getChatID(name: String): ChatID

    @Query("SELECT chat_name FROM ChatIDEntry WHERE chat_id = :id")
    fun getChatName(id: ChatID): String

    @Query("DELETE FROM ChatIDEntry")
    fun clearAll()
}