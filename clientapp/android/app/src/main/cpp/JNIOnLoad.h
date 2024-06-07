//
// Created by royna on 6/7/2024.
//

#ifndef TGBOT_CLIENT_JNIONLOAD_H
#define TGBOT_CLIENT_JNIONLOAD_H

#include <jni.h>
#include <cassert>
#include <string>

#define NATIVE_METHOD(fn, sig)                   \
      {                                          \
           .name = #fn,                          \
           .signature = sig,                     \
           .fnPtr = reinterpret_cast<void *>(fn),\
      }

#define NATIVE_METHOD_SZ(methods) sizeof(methods) / sizeof(JNINativeMethod)

static inline jint JNI_onLoadDef(JavaVM *vm, std::string cls, const JNINativeMethod methods[],
                                 size_t size) {
    JNIEnv *env;
    int rc = JNI_ERR;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return rc;
    }
    jclass c = env->FindClass(cls.c_str());
    if (c == nullptr) return rc;

    rc = env->RegisterNatives(c, methods, size);
    assert(rc == JNI_OK);
    return JNI_VERSION_1_6;
}

#endif //TGBOT_CLIENT_JNIONLOAD_H
