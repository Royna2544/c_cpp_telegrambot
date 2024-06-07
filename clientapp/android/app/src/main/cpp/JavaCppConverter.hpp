#pragma once

#include <jni.h>

#include <string>
#include <vector>


template <typename To, typename From>
To Convert(JNIEnv *env, const From str) = delete;

template <>
std::string Convert(JNIEnv *env, const jstring str) {
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
jstring Convert(JNIEnv *env, const std::string str) {
    if (env->ExceptionOccurred()) {
        return nullptr;
    }
    return env->NewStringUTF(str.c_str());
}

template <>
jstring Convert(JNIEnv *env, const char* str) {
    return Convert<jstring>(env, std::string(str));
}
