package com.royna.tgbotclient.datastore

import androidx.room.Database
import androidx.room.RoomDatabase

@Database(entities = [ChatIDEntry::class], version = 1)
abstract class ChatIDDatabase : RoomDatabase() {
    abstract fun impl(): IChatIDOperations
}