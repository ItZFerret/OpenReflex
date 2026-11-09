// crt_string.cpp -- oracle adapters for core/game/crt_string.c (cdecl, the result in EAX).
#include "crt_oracle.h"
using namespace reflex_port;

RX_CRT_PORT(0x7855aac0, Memcpy, OUT_EAX) { m.r[0] = uint32_t(crt_memcpy(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x7855afb0, Memmove, OUT_EAX) { m.r[0] = uint32_t(crt_memmove(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x7855b320, Memset, OUT_EAX) { m.r[0] = uint32_t(crt_memset(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x78544792, Memcmp, OUT_EAX) { m.r[0] = uint32_t(crt_memcmp(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x7855ae30, Memchr, OUT_EAX) { m.r[0] = uint32_t(crt_memchr(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x7855664b, Memcpy_s, OUT_EAX) { m.r[0] = uint32_t(crt_memcpy_s(m.arg(0), m.arg(1), m.arg(2), m.arg(3))); return m.ret(0); }
RX_CRT_PORT(0x785566cd, Memmove_s, OUT_EAX) { m.r[0] = uint32_t(crt_memmove_s(m.arg(0), m.arg(1), m.arg(2), m.arg(3))); return m.ret(0); }
RX_CRT_PORT(0x7855b660, Strlen, OUT_EAX) { m.r[0] = uint32_t(crt_strlen(m.arg(0))); return m.ret(0); }
RX_CRT_PORT(0x785569b2, Strcpy_s, OUT_EAX) { m.r[0] = uint32_t(crt_strcpy_s(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x7855b830, Strncpy, OUT_EAX) { m.r[0] = uint32_t(crt_strncpy(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x785571c9, Wcsncpy, OUT_EAX) { m.r[0] = uint32_t(crt_wcsncpy(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x7855b4b0, Strchr, OUT_EAX) { m.r[0] = uint32_t(crt_strchr(m.arg(0), m.arg(1))); return m.ret(0); }
RX_CRT_PORT(0x7855ba50, Strrchr, OUT_EAX) { m.r[0] = uint32_t(crt_strrchr(m.arg(0), m.arg(1))); return m.ret(0); }
RX_CRT_PORT(0x7855bb50, Strstr, OUT_EAX) { m.r[0] = uint32_t(crt_strstr(m.arg(0), m.arg(1))); return m.ret(0); }
RX_CRT_PORT(0x7855ba00, Strpbrk, OUT_EAX) { m.r[0] = uint32_t(crt_strpbrk(m.arg(0), m.arg(1))); return m.ret(0); }
RX_CRT_PORT(0x7855b580, Strcmp, OUT_EAX) { m.r[0] = uint32_t(crt_strcmp(m.arg(0), m.arg(1))); return m.ret(0); }
RX_CRT_PORT(0x78556789, Strncmp, OUT_EAX) { m.r[0] = uint32_t(crt_strncmp(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x78557165, Wcsncmp, OUT_EAX) {     // the count argument slot [esp+0xc] is counted down in place
    uint32_t left; m.r[0] = uint32_t(crt_wcsncmp_ex(m.arg(0), m.arg(1), m.arg(2), &left));
    m.mem.write(m.r[4] + 0xc, 4, left); return m.ret(0);
}
RX_CRT_PORT(0x7855e8c1, Tolower, OUT_EAX) { m.r[0] = uint32_t(crt_tolower(m.arg(0))); return m.ret(0); }
RX_CRT_PORT(0x78578d50, Stricmp, OUT_EAX) { m.r[0] = uint32_t(crt_stricmp(m.arg(0), m.arg(1))); return m.ret(0); }
RX_CRT_PORT(0x78579347, Strnicmp, OUT_EAX) { m.r[0] = uint32_t(crt_strnicmp(m.arg(0), m.arg(1), m.arg(2))); return m.ret(0); }
RX_CRT_PORT(0x7855e3af, Strtol, OUT_EAX) { m.r[0] = uint32_t(crt_strtol(m.arg(0), m.arg(1), int32_t(m.arg(2)))); return m.ret(0); }
RX_CRT_PORT(0x78546074, Atol, OUT_EAX) { m.r[0] = uint32_t(crt_atol(m.arg(0))); return m.ret(0); }
RX_CRT_PORT(0x785460ad, Atoi, OUT_EAX) { m.r[0] = uint32_t(crt_atoi(m.arg(0))); return m.ret(0); }
