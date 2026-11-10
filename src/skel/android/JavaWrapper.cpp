//
// Created by mrxenginner on 14/07/2025.
//

#include "common.h"
#include "JavaWrapper.h"

#if defined ANDROID

#include <pthread.h>

extern "C" JavaVM *javaVM;

namespace {
    // Every thread that used GetEnv() and had to be attached is remembered so
    // it can be detached before it exits; otherwise the VM keeps a JNIEnv for
    // a dead thread and the process aborts on some Android versions.
    pthread_key_t gAttachKey;
    pthread_once_t gAttachKeyOnce = PTHREAD_ONCE_INIT;

    void DetachOnThreadExit(void*)
    {
        if (javaVM == nullptr)
            return;
        JNIEnv* env = nullptr;
        jint status = javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (status == JNI_OK)
            javaVM->DetachCurrentThread();
    }

    void CreateAttachKey()
    {
        pthread_key_create(&gAttachKey, DetachOnThreadExit);
    }
}

JNIEnv* CJavaWrapper::GetEnv() {

    if (javaVM == nullptr) {
        debug("GetEnv: no JavaVM");
        return nullptr;
    }

    JNIEnv* env = nullptr;
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_6;
    args.name = const_cast<char*>("reVC");
    args.group = nullptr;

    jint getEnvStat = javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (getEnvStat == JNI_OK)
        return env;

    if (getEnvStat == JNI_EDETACHED) {
        if (javaVM->AttachCurrentThread(&env, &args) != 0) {
            debug("GetEnv: failed to attach the current thread");
            return nullptr;
        }
        pthread_once(&gAttachKeyOnce, CreateAttachKey);
        pthread_setspecific(gAttachKey, reinterpret_cast<void*>(1));
        return env;
    }

    if (getEnvStat == JNI_EVERSION)
        debug("GetEnv: version not supported");
    else
        debug("GetEnv: error %d", (int)getEnvStat);
    return nullptr;
}

void CJavaWrapper::DetachThread()
{
    if (javaVM == nullptr)
        return;
    JNIEnv* env = nullptr;
    if (javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK)
        javaVM->DetachCurrentThread();
}

void CJavaWrapper::ExitGame() {

    JNIEnv* env = GetEnv();

    if (env == nullptr) {
        debug("ExitGame: no env");
        return;
    }
    if (activity == nullptr || s_ExitGame == nullptr) {
        debug("ExitGame: no activity/exitGame method");
        return;
    }

    env->CallVoidMethod(activity, s_ExitGame);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

CJavaWrapper::CJavaWrapper(JNIEnv *env, jobject activity_)
    : s_ExitGame(nullptr)
    , activity(nullptr)
{
    if (env == nullptr || activity_ == nullptr) {
        debug("CJavaWrapper: null env/activity");
        return;
    }

    activity = env->NewGlobalRef(activity_);
    if (activity == nullptr) {
        debug("CJavaWrapper: NewGlobalRef failed");
        return;
    }

    // GetObjectClass returns a *local* reference that the caller cannot delete
    // for us: the old code deleted the caller's `activity` local ref instead,
    // which invalidated it for everyone else on the same JNI frame.
    jclass nvEventClass = env->GetObjectClass(activity);
    if (nvEventClass == nullptr) {
        debug("CJavaWrapper: no class");
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    s_ExitGame = env->GetMethodID(nvEventClass, "exitGame", "()V");
    if (s_ExitGame == nullptr) {
        // A missing method would otherwise be called and crash the game thread
        // when the player exits. Clear the pending NoSuchMethodError.
        if (env->ExceptionCheck()) env->ExceptionClear();
        debug("CJavaWrapper: exitGame()V is not implemented by the activity");
    }
    env->DeleteLocalRef(nvEventClass);
}

CJavaWrapper::~CJavaWrapper() {
    if (activity == nullptr)
        return;
    // Attaching here would be pointless work if the VM is already gone; and a
    // detached thread simply cannot delete a global reference, so let the VM
    // reclaim it in that (shutdown) case instead of crashing.
    JNIEnv* env = GetEnv();
    if (env != nullptr)
        env->DeleteGlobalRef(activity);
    activity = nullptr;
}

CJavaWrapper* g_pJavaWrapper = nullptr;
#endif
