#pragma once

template <typename T>
/**
* @brief A base class for creating a single instance of a class
* @tparam T the type of the instance
*/
struct InstanceClassBase {
    /**
    * @brief Get the single instance of the class
    * @return a reference to the instance
    */
    static T& getInstance() {
        static T instance;
        return instance;
    }
};