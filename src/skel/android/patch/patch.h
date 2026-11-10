//
// Created by mrxenginner on 14/07/2025.
//

#ifndef REVC_PATCH_H
#define REVC_PATCH_H

#if defined ANDROID

#include "common.h"
#include <dlfcn.h>
#include <link.h>
#include <string.h>

class Patch {
public:
	// Returns the load base of the given library, or 0 when it cannot be
	// resolved. Resolves by soname through dl_iterate_phdr so no extra
	// reference (and no leaked dlopen handle) is created.
	static uintptr_t FindLib(const char* libName)
	{
		if (libName == nullptr)
			return 0;

		struct Search {
			const char* name;
			uintptr_t base;
		} search = { libName, 0 };

		dl_iterate_phdr([](struct dl_phdr_info* info, size_t, void* data) -> int {
			Search* s = static_cast<Search*>(data);
			if (info->dlpi_name == nullptr || info->dlpi_name[0] == '\0')
				return 0; // the executable itself
			// dlpi_name is a full path; compare only the file name part.
			const char* slash = strrchr(info->dlpi_name, '/');
			const char* fileName = slash != nullptr ? slash + 1 : info->dlpi_name;
			if (strcmp(fileName, s->name) == 0) {
				s->base = static_cast<uintptr_t>(info->dlpi_addr);
				return 1; // stop iterating
			}
			return 0;
		}, &search);

		if (search.base != 0)
			return search.base;

		// Fall back to the name the platform knows the library by (Android
		// linkers report it as librevc.so). The handle is intentionally kept
		// open: dlclose() of a library that is still executing is undefined.
		void* handle = dlopen(libName, RTLD_LAZY | RTLD_NOLOAD);
		if (handle == nullptr)
			return 0;
		Dl_info info;
		if (dladdr(handle, &info) != 0)
			return reinterpret_cast<uintptr_t>(info.dli_fbase);
		return 0;
	}
};
#endif

#endif // REVC_PATCH_H
