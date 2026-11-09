//
// Created by mrxenginner on 14/07/2025.
//

#ifndef REVC_CJAVAWRAPPER_H
#define REVC_CJAVAWRAPPER_H

#if defined ANDROID

#include <jni.h>
#include <string>

#define EXCEPTION_CHECK(env) \
	if ((env)->ExceptionCheck()) \
	{ \
		(env)->ExceptionDescribe(); \
		(env)->ExceptionClear(); \
		return; \
	}

class CJavaWrapper
{
    jmethodID s_ExitGame;
public:
    // Global reference to the Java activity; never null while the object is
    // alive. Owned by this object (NewGlobalRef/DeleteGlobalRef).
    jobject activity;

    CJavaWrapper(JNIEnv* env, jobject activity);
    ~CJavaWrapper();

    CJavaWrapper(const CJavaWrapper&) = delete;
    CJavaWrapper& operator=(const CJavaWrapper&) = delete;

    void ExitGame();

    // static methods
    static JNIEnv* GetEnv();
    static void DetachThread();
};

extern CJavaWrapper* g_pJavaWrapper;
#endif

#endif // REVC_CJAVAWRAPPER_H
