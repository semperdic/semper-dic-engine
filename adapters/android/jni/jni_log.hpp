#ifndef SEMPER_JNI_LOG_HPP
#define SEMPER_JNI_LOG_HPP

// Local logging for the JNI adapter. Kept self-contained (no engine-internal
// headers) so this adapter can live in the app repo and link only the *public*
// engine — see docs/engine/ENGINE_APP_CONTRACT.md.

#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG "Semper"
#endif

#ifndef LOGD
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#endif
#ifndef LOGE
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif

#endif  // SEMPER_JNI_LOG_HPP
