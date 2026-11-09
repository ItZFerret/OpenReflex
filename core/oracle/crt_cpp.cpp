// crt_cpp.cpp -- oracle adapters for core/game/crt_cpp.c: the msvcp90 std::string members the exe imports (thiscall: ECX =
// the string, callee-cleaned arguments; the operators and _Copy_s are cdecl).
#include "crt_oracle.h"
using namespace reflex_port;

RX_CRT_PORT(0x78485ab4, StringEnd, OUT_EAX) { m.r[0] = crt_str_end(m.r[1], m.arg(0)); return m.ret(4); }
RX_CRT_PORT(0x784958f7, StringEqCstr, OUT_EAX) { m.r[0] = crt_str_eq_cstr(m.arg(0), m.arg(1)); return m.ret(0); }
RX_CRT_PORT(0x78495979, StringLess, OUT_EAX) { m.r[0] = crt_str_less(m.arg(0), m.arg(1)); return m.ret(0); }
RX_CRT_PORT(0x78485c03, StringFindStr, OUT_EAX) { m.r[0] = crt_str_find_str(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_CRT_PORT(0x7848540c, StringFindCstr, OUT_EAX) { m.r[0] = crt_str_find_cstr(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_CRT_PORT(0x78486aff, StringReplaceN, OUT_EAX) { m.r[0] = crt_str_replace_n(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(16); }
RX_CRT_PORT(0x78486c52, StringReplaceCstr, OUT_EAX) { m.r[0] = crt_str_replace_cstr(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_CRT_PORT(0x784874d8, StringReplace, OUT_EAX) { m.r[0] = crt_str_replace(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_CRT_PORT(0x78485f79, StringClear, 0) { crt_str_clear(m.r[1]); return m.ret(0); }
RX_CRT_PORT(0x78486428, StringAssign, OUT_EAX) { m.r[0] = crt_str_assign(m.r[1], m.arg(0)); return m.ret(4); }
RX_CRT_PORT(0x784863af, StringAssignN, OUT_EAX) { m.r[0] = crt_str_assign_n(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_CRT_PORT(0x78486316, StringAssignSub, OUT_EAX) { m.r[0] = crt_str_assign_sub(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_CRT_PORT(0x78484f13, TraitsCopy_s, OUT_EAX) { m.r[0] = crt_traits_copy_s(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0); }
RX_CRT_PORT(0x784872a2, StringOperatorAssign, OUT_EAX) { m.r[0] = crt_str_operator_assign(m.r[1], m.arg(0)); return m.ret(4); }
RX_CRT_PORT(0x78485912, AllocatorAllocate, OUT_EAX) { m.r[0] = crt_allocator_allocate(m.r[1], m.arg(0)); return m.ret(4); }
RX_CRT_PORT(0x78485e6a, StringCtor, OUT_EAX) { m.r[0] = crt_str_ctor(m.r[1]); return m.ret(0); }
RX_CRT_PORT(0x78487135, StringCtorCstr, OUT_EAX) { m.r[0] = crt_str_ctor_cstr(m.r[1], m.arg(0)); return m.ret(4); }
RX_CRT_PORT(0x78487272, StringCtorCopy, OUT_EAX) { m.r[0] = crt_str_ctor_copy(m.r[1], m.arg(0)); return m.ret(4); }
RX_CRT_PORT(0x78485ea4, StringDtor, 0) { crt_str_dtor(m.r[1]); return m.ret(0); }
