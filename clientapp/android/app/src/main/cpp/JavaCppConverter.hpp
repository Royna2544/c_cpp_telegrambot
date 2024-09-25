#pragma once

#include <jni.h>

#include <string>
#include <vector>


template <typename To, typename From>
To Convert(JNIEnv *env, const From str) = delete;

template <>
std::string Convert(JNIEnv *env, jstring str) {
    jboolean isCopy;

    if (env->ExceptionOccurred()) {
        return {};
    }
    const char *convertedValue = env->GetStringUTFChars(str, &isCopy);
    std::string string = std::string(convertedValue);
    env->ReleaseStringUTFChars(str, convertedValue);
    return string;
}

template <>
jstring Convert(JNIEnv *env, std::string_view str) {
    if (env->ExceptionOccurred()) {
        return nullptr;
    }
    return env->NewStringUTF(str.data());
}

template <>
jstring Convert(JNIEnv* env, const char * __restrict str) {
    return Convert<jstring, std::string_view>(env, str);
}

// Use pointer to avoid unnecessary copy
template <>
jstring Convert(JNIEnv* env, std::string* str) {
    return Convert<jstring, std::string_view>(env, *str);
}