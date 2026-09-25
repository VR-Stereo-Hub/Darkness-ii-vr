// core/util/ini.h - small helpers around the Win32 private-profile API.
// Never called under the loader lock (the profile API can pull in other DLLs);
// the config is read from the first Direct3DCreate9(Ex) call.
#pragma once

namespace d2vr::ini {
float read_float(const char* ini, const char* section, const char* key, float def);
int   read_int(const char* ini, const char* section, const char* key, int def);
void  read_string(const char* ini, const char* section, const char* key, const char* def, char* out, int cap);
bool  write_text_file(const char* path, const char* text);
} // namespace d2vr::ini
