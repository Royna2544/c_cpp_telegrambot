package com.royna.tgbotclient.datastore

/**
 * IDataStore interface defines the contract for a key-value data store.
 *
 * @param K the type of keys maintained by this data store.
 * @param V the type of mapped values.
 */
interface IDataStore<K, V> {

    /**
     * Creates the data store.
     *
     * @return `true` if the data store is successfully created, `false` otherwise.
     */
    fun create() : Boolean

    /**
     * Writes the specified value with the specified key in the data store.
     *
     * @param key the key with which the specified value is to be associated.
     * @param value the value to be associated with the specified key.
     * @return `true` if the value is successfully written, `false` otherwise.
     */
    fun write(key: K, value: V) : Boolean

    /**
     * Deletes the value associated with the specified key in the data store.
     *
     * @param key the key whose associated value is to be deleted.
     * @return `true` if the value is successfully deleted, `false` otherwise.
     */
    fun delete(key: K) : Boolean

    /**
     * Reads all key-value pairs from the data store.
     *
     * @return a map containing all key-value pairs in the data store.
     */
    fun readAll() : Map<K, V>

    /**
     * Clears all key-value pairs from the data store.
     *
     * @return `true` if the data store is successfully cleared, `false` otherwise.
     */
    fun clearAll() : Boolean

    /**
     * Reads the value associated with the specified key from the data store.
     *
     * @param key the key whose associated value is to be read.
     * @return the value associated with the specified key, or `null` if the key does not exist.
     */
    fun read(key: K) : V? {
        readAll().forEach {
            if (it.key == key) {
                return it.value
            }
        }
        return null
    }

    /**
     * Finds the key associated with the specified value in the data store.
     *
     * @param value the value whose associated key is to be found.
     * @return the key associated with the specified value, or `null`
     * if the value does not exist in the data store.
     */
    fun findKey(value: V) : K? {
        readAll().forEach {
            if (it.value == value) {
                return it.key
            }
        }
        return null
    }
}
