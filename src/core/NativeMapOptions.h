#ifndef __REVC_NATIVE_MAP_OPTIONS_H__
#define __REVC_NATIVE_MAP_OPTIONS_H__

#include <cctype>
#include <cstring>

// Only GTA_VC.DAT IDE/IPL directives under DATA/MAPS belong to the native
// island. DEFAULT.DAT, collision, paths and every custom/ ZIP or IMG resource
// must still load when the native island is disabled.
inline bool
IsNativeVCMapFile(const char *path)
{
    if(path == nullptr) return false;
    while(*path == ' ' || *path == '\t') path++;
    while(path[0] == '.' && (path[1] == '/' || path[1] == '\\')) path += 2;
    while(*path == '/' || *path == '\\') path++;
    const char prefix[] = "data/maps/";
    for(size_t i = 0; i < sizeof(prefix) - 1; i++) {
        unsigned char c = (unsigned char)path[i];
        if(!c || (prefix[i] == '/' ? (c != '/' && c != '\\') :
                  std::tolower(c) != prefix[i]))
            return false;
    }
    const char *file = path + sizeof(prefix) - 1;
    if(!*file) return false;
    const char *ext = strrchr(file, '.');
    if(ext == nullptr || strlen(ext) < 4 ||
       std::tolower((unsigned char)ext[1]) != 'i' ||
       (ext[4] != '\0' && ext[4] != ' ' && ext[4] != '\t')) return false;
    return (std::tolower((unsigned char)ext[2]) == 'd' &&
            std::tolower((unsigned char)ext[3]) == 'e') ||
           (std::tolower((unsigned char)ext[2]) == 'p' &&
            std::tolower((unsigned char)ext[3]) == 'l');
}

#endif
