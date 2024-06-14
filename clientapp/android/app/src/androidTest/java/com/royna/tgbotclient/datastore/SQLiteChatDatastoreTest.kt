package com.royna.tgbotclient.datastore

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.royna.tgbotclient.datastore.chat.SQLiteChatDatastore
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertTrue

class SQLiteChatDatastoreTest {
    lateinit var mContext: Context
    lateinit var mChatDatastore: SQLiteChatDatastore

    @Before
    fun setUp() {
        mContext = ApplicationProvider.getApplicationContext()
        mChatDatastore = SQLiteChatDatastore(mContext)
    }

    @After
    fun tearDown() {
        mChatDatastore.close()
        mContext.getDatabasePath(SQLiteChatDatastore.DATABASE_NAME).delete()
    }

    @Test
    fun delete() {
        assertTrue(mChatDatastore.write(kChatName, kChatId))
        assertTrue(mChatDatastore.delete(kChatName))
        assertFalse(mChatDatastore.delete(kChatName))
    }

    @Test
    fun readwrite() {
        assertTrue(mChatDatastore.write(kChatName, kChatId))
        assertEquals(mChatDatastore.read(kChatName), kChatId)

        // Fail because of unique constraint
        assertFalse(mChatDatastore.write(kChatName, kOtherChatId))

        assertEquals(mChatDatastore.read(kChatName), kChatId)
    }

    @Test
    fun invalidRead() {
        assertEquals(mChatDatastore.read(kOtherChatName), null)
    }

    @Test
    fun readAll() {
        assertTrue(mChatDatastore.write(kChatName, kChatId))
        assertTrue(mChatDatastore.write(kOtherChatName, kOtherChatId))
        assertEquals(mChatDatastore.readAll(), mapOf(kChatName to kChatId,
            kOtherChatName to kOtherChatId))
    }
    companion object {
        private const val kChatName = "Blex"
        private const val kOtherChatName = "Clex"
        private const val kChatId = -1000010100L
        private const val kOtherChatId = -1000010101L
    }
}