package com.royna.tgbotclient.datastore

import android.content.Context
import androidx.room.Room
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent

@Module
@InstallIn(SingletonComponent::class)
class ChatIDModule {
    @Provides
    fun provideChatIDOperations(database: ChatIDDatabase): IChatIDOperations
        = database.impl()

    @Provides
    fun provideChatIDDatabase(@ApplicationContext context: Context): ChatIDDatabase = Room.databaseBuilder(
        context,
        ChatIDDatabase::class.java,
        "chatid.db"
    ).build()
}