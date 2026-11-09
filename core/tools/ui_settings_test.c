/* ui_settings_test.c -- rx_session_ui_setting (include/rx_ui_settings.h) in the product session.
 *   ui_settings_test <install root>
 *   build: core/build.sh, then clang -std=c11 -O1 -Iinclude -Isrc -Igame -o <out> tools/ui_settings_test.c (every object in build/obj) -lz -lc++
 * Creates a session, loads the default event (the guest profile: UISettings_Defaults) and checks the read, the write and what the game's
 * UISettings_Apply does with it: MouseSensitivity * 0.05 clamped [0.1, 5] into 0x009cb7b0, the pad vibration enable, the stored values of
 * CareerDifficulty / Handedness / ControllerMotion, an unknown name and bad arguments. Exit 0 = every check passed. */
#include "rx_api.h"
#include "rx_ui_settings.h"
#include "../src/rx_mem.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(cond, ...) do { if (!(cond)) { g_fail++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int32_t get(rx_session* s, const char* name) { int32_t v = -12345; int rc = rx_session_ui_setting(s, name, 0, 0, &v); if (rc) printf("  get %s -> %d\n", name, rc); return v; }

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: ui_settings_test <install root>\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    { int32_t b = -9; int rc = rx_session_ui_setting(s, "Vibration", 0, 0, &b); printf("  before the event load: read -> %d (value %d)\n", rc, b); rc = rx_session_ui_setting(s, "MouseSensitivity", 77, 1, &b); printf("  set before load -> %d (was %d)\n", rc, b); }
    rx_event_spec spec; rx_session_event_default(&spec);
    if (rx_session_event_load(s, &spec) != 0) { printf("event load: %s\n", rx_session_last_error(s)); return 1; }
    /* defaults (UISettings_Defaults 005b8030) */
    printf("  after the load MouseSensitivity = %d\n", get(s, "MouseSensitivity"));
    rx_session_ui_setting(s, "MouseSensitivity", 50, 1, NULL);
    CHECK(get(s, "Vibration") == 0, "Vibration default");
    CHECK(get(s, "Handedness") == 1, "Handedness default (right)");
    CHECK(get(s, "ControllerMotion") == 0, "ControllerMotion default");
    CHECK(get(s, "EffectsVolume") == 100 && get(s, "EngineVolume") == 100, "volume defaults");
    float sens; { uint32_t b = U32(0, 0x009cb7b0); memcpy(&sens, &b, 4); }
    CHECK(fabsf(sens - 2.5f) < 1e-5f, "sensitivity factor %g (50 * 0.05)", (double)sens);
    /* writes */
    int32_t before = -1;
    CHECK(rx_session_ui_setting(s, "MouseSensitivity", 80, 1, &before) == 0 && before == 50, "set MouseSensitivity (before %d)", before);
    CHECK(get(s, "MouseSensitivity") == 80, "MouseSensitivity read back");
    { uint32_t b = U32(0, 0x009cb7b0); memcpy(&sens, &b, 4); }
    CHECK(fabsf(sens - 4.0f) < 1e-5f, "sensitivity factor %g after 80 (4.0)", (double)sens);
    CHECK(rx_session_ui_setting(s, "mousesensitivity", 100, 1, NULL) == 0, "case-insensitive set");
    { uint32_t b = U32(0, 0x009cb7b0); memcpy(&sens, &b, 4); }
    CHECK(fabsf(sens - 5.0f) < 1e-5f, "sensitivity factor %g after 100 (clamped 5.0)", (double)sens);
    CHECK(rx_session_ui_setting(s, "CareerDifficulty", 120, 1, &before) == 0 && get(s, "CareerDifficulty") == 120, "CareerDifficulty (before %d)", before);
    CHECK(rx_session_ui_setting(s, "Handedness", 0, 1, NULL) == 0 && get(s, "Handedness") == 0, "Handedness left");
    CHECK(rx_session_ui_setting(s, "ControllerMotion", 1, 1, NULL) == 0 && get(s, "ControllerMotion") == 1, "ControllerMotion");
    CHECK(rx_session_ui_setting(s, "Vibration", 1, 1, NULL) == 0 && get(s, "Vibration") == 1, "Vibration on");
    CHECK(rx_session_ui_setting(s, "NoSuchSetting", 1, 1, NULL) == 2, "unknown name");
    CHECK(rx_session_ui_setting(NULL, "Vibration", 1, 1, NULL) == 1, "no session");
    CHECK(rx_session_ui_setting(s, NULL, 1, 1, NULL) == 1, "no name");
    printf(g_fail ? "FAIL (%d)\n" : "PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
