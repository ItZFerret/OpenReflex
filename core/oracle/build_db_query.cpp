// build_db_query.cpp -- oracle adapter for core/game/build_db_query.c (group rx_build_asset).
#include "oracle.h"
extern "C" {
#include "../game/build_asset.h"
}
using namespace reflex_port;

// 0084a840 Database::vf9 (thiscall db, database name, package name; RET 8) -> EAX (bool)
RX_PORT(0x0084a840, DatabaseVf9, "rx_build_asset", OUT_EAX) { m.r[0] = rx_database_vf_package_known(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }

// 0084c090 Database::vf11 (thiscall) -> EAX (1 = idle); SetEvent of the wake event runs natively here as in the original (the emulated worker runs inside it)
RX_PORT(0x0084c090, DatabaseVf11, "rx_build_asset", OUT_EAX) { m.r[0] = rx_database_vf_idle(m.r[1]); return m.ret(0); }
