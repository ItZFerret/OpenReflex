// Namespace-isolated inclusion of the unchanged, reviewed private ray source.
#include "../../src/fp32.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
namespace reflex_controls_terrain_ray_private {
#define REFLEX_TERRAIN_RAY_NO_MAIN
#include "../terrain_ray/terrain_ray_draft.cpp"
}
