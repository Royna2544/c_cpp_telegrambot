package com.royna.tgbotclient.datastore
import androidx.test.ext.junit.runners.AndroidJUnit4
import dagger.hilt.android.testing.HiltAndroidRule
import dagger.hilt.android.testing.HiltAndroidTest
import dagger.hilt.android.testing.UninstallModules
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import javax.inject.Inject

@RunWith(AndroidJUnit4::class)
@UninstallModules(ChatIDModule::class)
@HiltAndroidTest
class ChatIDOperationsTest {

    @get:Rule
    var hiltRule = HiltAndroidRule(this)

    @Inject
    lateinit var database: ChatIDDatabase

    @Inject
    lateinit var dao: IChatIDOperations

    @Before
    fun setup() {
        hiltRule.inject()
    }

    @After
    fun tearDown() {
        database.close()
    }

    @Test
    fun testAddAndRetrieveChatIDEntry() {
        val entry = ChatIDEntry(kChatId,kChatName)
        dao.add(entry)

        val entries = dao.getAll()
        Assert.assertEquals(1, entries.size)
        Assert.assertEquals(kChatName, entries[0].name)
        Assert.assertEquals(kChatId, entries[0].id)
    }

    @Test
    fun testRemoveChatIDEntry() {
        val entry = ChatIDEntry(kChatId,kChatName)
        dao.add(entry)
        dao.remove(entry)

        val entries = dao.getAll()
        Assert.assertTrue(entries.isEmpty())
    }

    @Test
    fun testGetChatIDByName() {
        val entry = ChatIDEntry(kChatId,kChatName)
        dao.add(entry)

        val chatID = dao.getChatID(kChatName)
        Assert.assertEquals(kChatId, chatID)
    }

    @Test
    fun testGetChatNameByID() {
        val entry = ChatIDEntry(kChatId,kChatName)
        dao.add(entry)

        val chatName = dao.getChatName(kChatId)
        Assert.assertEquals(kChatName, chatName)
    }

    @Test
    fun testClearAll() {
        val entry1 = ChatIDEntry(kChatId,kChatName)
        val entry2 = ChatIDEntry(kOtherChatId, kOtherChatName)
        dao.add(entry1, entry2)

        dao.clearAll()
        val entries = dao.getAll()
        Assert.assertTrue(entries.isEmpty())
    }

    companion object {
        private const val kChatName = "Blex"
        private const val kOtherChatName = "Clex"
        private const val kChatId = -1000010100L
        private const val kOtherChatId = -1000010101L
    }
}
