package com.royna.tgbotclient

import android.content.Context
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import com.royna.tgbotclient.datastore.ChatIDDatabase
import com.royna.tgbotclient.datastore.IChatIDOperations
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object DaggerModules {
    @Provides
    @Singleton
    fun provideApplicationContext() : Context {
        return ApplicationProvider.getApplicationContext()
    }
    @Provides
    @Singleton
    fun provideChatDatastore(context: Context) : ChatIDDatabase {
        return Room.inMemoryDatabaseBuilder(
            context,
            ChatIDDatabase::class.java
        ).build()
    }
    @Provides
    @Singleton
    fun provideChatIDOperations(database: ChatIDDatabase) : IChatIDOperations {
        return database.impl()
    }

}