// crt_printf.cpp -- oracle adapters for core/game/crt_printf.c (cdecl; the count in EAX).
#include "crt_oracle.h"
using namespace reflex_port;

RX_CRT_PORT(0x78552b7e, Sprintf, OUT_EAX) { m.r[0] = uint32_t(crt_sprintf(m.arg(0), m.arg(1), m.r[4] + 12)); return m.ret(0); }
RX_CRT_PORT(0x7855471e, Vsnprintf, OUT_EAX) { m.r[0] = uint32_t(crt_vsnprintf(m.arg(0), m.arg(1), m.arg(2), m.arg(3))); return m.ret(0); }
RX_CRT_PORT(0x78554666, Vsnprintf_l, OUT_EAX) {
    m.r[0] = uint32_t(crt_vsnprintf_l(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4))); return m.ret(0);
}
