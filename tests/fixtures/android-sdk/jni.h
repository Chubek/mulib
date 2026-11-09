/* MuLIB-owned C test double, NOT a JNI ABI header and never installed.
 * Real-header compatibility is checked separately with an installed JDK/NDK. */
#ifndef MULIB_TEST_JNI_H
#define MULIB_TEST_JNI_H
#include <stdint.h>
#define JNI_OK 0
#define JNI_EDETACHED (-2)
#define JNI_VERSION_1_6 0x00010006
#define JNICALL
typedef uint8_t jboolean;
typedef int8_t jbyte;
typedef uint16_t jchar;
typedef int16_t jshort;
typedef int32_t jint;
typedef int64_t jlong;
typedef float jfloat;
typedef double jdouble;
typedef jint jsize;
typedef void *jobject;
typedef jobject jclass;
typedef jobject jstring;
typedef jobject jarray;
typedef jobject jbyteArray;
typedef void *jmethodID;
typedef void *jfieldID;
typedef union jvalue { jboolean z; jbyte b; jchar c; jshort s; jint i; jlong j; jfloat f; jdouble d; jobject l; } jvalue;
typedef struct JNINativeMethod { char *name, *signature; void *fnPtr; } JNINativeMethod;
typedef const struct JNINativeInterface *JNIEnv;
typedef const struct JNIInvokeInterface *JavaVM;
struct JNINativeInterface {
  jboolean (*ExceptionCheck)(JNIEnv *);
  void (*ExceptionClear)(JNIEnv *);
  jint (*PushLocalFrame)(JNIEnv *, jint);
  jobject (*PopLocalFrame)(JNIEnv *, jobject);
  jobject (*NewGlobalRef)(JNIEnv *, jobject);
  void (*DeleteGlobalRef)(JNIEnv *, jobject);
  void (*DeleteLocalRef)(JNIEnv *, jobject);
  jclass (*FindClass)(JNIEnv *, const char *);
  jmethodID (*GetStaticMethodID)(JNIEnv *, jclass, const char *, const char *);
  jmethodID (*GetMethodID)(JNIEnv *, jclass, const char *, const char *);
#define MULIB_TEST_JNI_CALLS_(Type,type) \
  type (*CallStatic##Type##MethodA)(JNIEnv *, jclass, jmethodID, const jvalue *); \
  type (*Call##Type##MethodA)(JNIEnv *, jobject, jmethodID, const jvalue *);
  MULIB_TEST_JNI_CALLS_(Boolean, jboolean)
  MULIB_TEST_JNI_CALLS_(Byte, jbyte)
  MULIB_TEST_JNI_CALLS_(Char, jchar)
  MULIB_TEST_JNI_CALLS_(Short, jshort)
  MULIB_TEST_JNI_CALLS_(Int, jint)
  MULIB_TEST_JNI_CALLS_(Long, jlong)
  MULIB_TEST_JNI_CALLS_(Float, jfloat)
  MULIB_TEST_JNI_CALLS_(Double, jdouble)
  MULIB_TEST_JNI_CALLS_(Object, jobject)
  MULIB_TEST_JNI_CALLS_(Void, void)
#undef MULIB_TEST_JNI_CALLS_
  jstring (*NewString)(JNIEnv *, const jchar *, jsize);
  jsize (*GetStringLength)(JNIEnv *, jstring);
  void (*GetStringRegion)(JNIEnv *, jstring, jsize, jsize, jchar *);
  jbyteArray (*NewByteArray)(JNIEnv *, jsize);
  jsize (*GetArrayLength)(JNIEnv *, jarray);
  void (*SetByteArrayRegion)(JNIEnv *, jbyteArray, jsize, jsize, const jbyte *);
  void (*GetByteArrayRegion)(JNIEnv *, jbyteArray, jsize, jsize, jbyte *);
  jfieldID (*GetStaticFieldID)(JNIEnv *, jclass, const char *, const char *);
  jint (*GetStaticIntField)(JNIEnv *, jclass, jfieldID);
  jint (*RegisterNatives)(JNIEnv *, jclass, const JNINativeMethod *, jint);
};
struct JNIInvokeInterface {
  jint (*GetEnv)(JavaVM *, void **, jint);
  jint (*AttachCurrentThread)(JavaVM *, void **, void *);
  jint (*DetachCurrentThread)(JavaVM *);
};
#endif
