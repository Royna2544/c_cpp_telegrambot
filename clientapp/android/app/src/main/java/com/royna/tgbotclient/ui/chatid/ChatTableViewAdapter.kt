package com.royna.tgbotclient.ui.chatid

import android.annotation.SuppressLint
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.royna.tgbotclient.R
import com.royna.tgbotclient.util.Logging

class ChatTableViewAdapter : RecyclerView.Adapter<ChatTableViewAdapter.ViewHolder>() {

    private var mLists : List<ChatTableViewItem> = emptyList()
    class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val mIndexView: TextView = view.findViewById(R.id.list_item_datastore_chat_index)
        val mNameView : TextView = view.findViewById(R.id.list_item_datastore_chat_name)
        val mIdView: TextView = view.findViewById(R.id.list_item_datastore_chat_id)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.list_item_datastore_chat, parent, false)
        return ViewHolder(view)
    }

    override fun getItemCount(): Int {
        return mLists.size
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        mLists[position].let {
            holder.mIndexView.text = it.index.toString()
            holder.mNameView.text = it.chatName
            holder.mIdView.text = it.chatId.toString()
        }
    }

    fun addItem(item: ChatTableViewItem) {
        Logging.debug("addItem: $item, mListSize: ${mLists.size}")
        item.index = mLists.size + 1
        mLists += item
        Logging.debug("notifyItemInserted: ${mLists.size - 1}")
        notifyItemInserted(mLists.size - 1)
    }

    fun removeItem(item: ChatTableViewItem) {
        val maybeIndex = mLists.find {
            it.chatId == item.chatId && it.chatName == item.chatName
        }
        if (maybeIndex != null) {
            val index = mLists.indexOf(maybeIndex)
            mLists = mLists.toMutableList().also {
                it.removeAt(index)
            }
            notifyItemRemoved(index)
        } else {
            Logging.warn("item not found, item: $item")
        }
    }

    fun insertItem(item: ChatTableViewItem, index: Int) {
        mLists = mLists.toMutableList().also {
            it.add(index, item)
        }
        notifyItemChanged(index)
    }

    @SuppressLint("NotifyDataSetChanged")
    fun clearAll() {
        mLists = emptyList()
        notifyDataSetChanged()
    }
}