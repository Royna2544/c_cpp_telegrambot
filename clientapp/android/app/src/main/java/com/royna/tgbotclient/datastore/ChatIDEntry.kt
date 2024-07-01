package com.royna.tgbotclient.datastore

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.Index
import androidx.room.PrimaryKey

@Entity(indices = [Index(value = ["chat_name"], unique = true)])
data class ChatIDEntry (
    @PrimaryKey
    @ColumnInfo(name="chat_id")
    val id: ChatID,

    @ColumnInfo(name="chat_name")
    val name: String
)

typealias ChatID = Long