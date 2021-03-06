#include <windows.h>
#include "jni.h"
#include "classes.h"
#include "loader.h"

DWORD WINAPI main_thread(LPVOID) {
	const auto jvm_dll = GetModuleHandleA("jvm.dll");
	if (!jvm_dll) {
		MessageBox(nullptr, L"Can't find jvm.dll module handle", L"ELoader", MB_OK | MB_ICONERROR);
		return 1;
	}

	const auto jni_get_created_vms_pointer = GetProcAddress(jvm_dll, "JNI_GetCreatedJavaVMs");
	if (!jni_get_created_vms_pointer) {
		MessageBox(nullptr, L"Can't find JNI_GetCreatedJavaVMs proc handle", L"ELoader", MB_OK | MB_ICONERROR); 
		return 1;
	}

	typedef jint(JNICALL * jni_get_created_java_vms_type)(JavaVM**, jsize, jsize*);
	const auto jni_get_created_java_vms_proc = jni_get_created_java_vms_type(jni_get_created_vms_pointer);
	auto n_vms = 1;
	JavaVM* jvm = nullptr;
	jni_get_created_java_vms_proc(&jvm, n_vms, &n_vms);

	if (n_vms == 0)	{
		MessageBox(nullptr, L"JVM not found!", L"ELoader", MB_OK | MB_ICONERROR); 
		return 1;
	}

	JNIEnv* jni_env = nullptr;
	jvm->AttachCurrentThread(reinterpret_cast<void **>(&jni_env), nullptr);
	jvm->GetEnv(reinterpret_cast<void **>(&jni_env), JNI_VERSION_1_8);
	if (!jni_env) {
		MessageBox(nullptr, L"Can't attach to JNIEnv", L"ELoader", MB_OK | MB_ICONERROR);
		jvm->DetachCurrentThread();
		return 1;
	}

	const auto loader_clazz = jni_env->DefineClass(nullptr, nullptr, reinterpret_cast<jbyte*>(class_loader_class), class_loader_class_size);
	if (!loader_clazz) {
		MessageBox(nullptr, L"Error on class defining", L"ELoader", MB_OK | MB_ICONERROR);
		jvm->DetachCurrentThread();
		return 1;
	}

	const jobjectArray classes_data = jobjectArray(jni_env->CallStaticObjectMethod(loader_clazz,
	                                                                               jni_env->GetStaticMethodID(loader_clazz, "getByteArray", "(I)[[B"),
	                                                                               jint(classes_count)));

	auto class_ptr = 0;
	for (auto j = 0; j < classes_count; j++) {
		jbyteArray class_byte_array = jni_env->NewByteArray(jsize(classes_sizes[j]));
		jni_env->SetByteArrayRegion(class_byte_array, 0, jsize(classes_sizes[j]), reinterpret_cast<jbyte *>(classes + class_ptr));
		class_ptr += classes_sizes[j];
		jni_env->SetObjectArrayElement(classes_data, j, class_byte_array);
	}

	const auto inject_result = jni_env->CallStaticIntMethod(loader_clazz, jni_env->GetStaticMethodID(loader_clazz, "injectCP", "([[B)I"), classes_data);
	if (inject_result) {
		MessageBox(nullptr, L"Error on injecting: injectResult != 0", L"ELoader", MB_OK | MB_ICONERROR);
		jvm->DetachCurrentThread();
		return 1;
	}
	jvm->DetachCurrentThread();
	return 0;
}

BOOL APIENTRY dll_main(HMODULE, const DWORD reason, LPVOID) {
	if (reason != DLL_PROCESS_ATTACH)
		return TRUE;
	CreateThread(nullptr, 0, main_thread, nullptr, 0, nullptr);
	return TRUE;
}
