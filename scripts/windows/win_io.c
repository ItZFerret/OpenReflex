/* Windows shim, linked into reflexworld.dll and reflex_boot.exe by build_plugin_windows.sh (no source of native/ is edited).
 *
 * POSIX open() is binary; the Windows CRT's open() is TEXT mode unless O_BINARY is given (CRLF -> LF on read, LF -> CRLF on write,
 * Ctrl-Z ends a read). boot.cpp's guest file I/O (the game's own reads / writes of its install and save files) and bink.cpp open
 * binary files with plain open(path, O_RDONLY...), so without this every original file read on Windows would be corrupted.
 * This definition of open() takes precedence over the CRT's (mingw-w64 declares it as a plain function in io.h, and its import
 * library's stub is only used when no object defines it); it is local to the DLL (not exported: no dllexport) and does not change
 * the process-wide _fmode (Unity's own CRT use is untouched).
 *
 * Paths stay narrow (the ANSI code page), as everywhere else in the plugin (fopen / stat / mkdir / opendir): see
 * _re/notes/client-windows.md "Paths".
 */
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <sys/stat.h>

int __cdecl open(const char *path, int flags, ...)
{
	int mode = 0;
	if (flags & _O_CREAT) {
		va_list a;
		va_start(a, flags);
		mode = va_arg(a, int);
		va_end(a);
	}
	if (!(flags & _O_TEXT)) flags |= _O_BINARY;
	/* POSIX permission bits (0644) -> the CRT's two (the UCRT rejects any other bit as an invalid parameter) */
	mode &= _S_IREAD | _S_IWRITE;
	return _open(path, flags, mode);
}
#endif
