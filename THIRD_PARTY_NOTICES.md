# Third-party notices

OPEN REFLEX is licensed under the GNU General Public License version 3 (LICENSE). It includes or uses the following third-party
material under its own terms; these licenses are compatible with distributing the project under GPL-3.0.

| Component | Where | License |
|---|---|---|
| Zstandard (zstd) | `core/src/zstd` | BSD 3-Clause (or GPLv2), see `core/src/zstd/LICENSE` |
| LZX decoder (from MonoGame) | `Unity/Reflex/Assets/Scripts/Content/LzxDecoder.cs` | dual LGPL 2.1 / Ms-PL, used under LGPL 2.1, see `LZX-LICENSE.txt` beside it |
| Poly Haven HDRI skies | `Unity/Reflex/Assets/ThirdParty/Skies` | CC0 1.0, see `LICENSES.md` there |
| Barlow / Barlow Condensed fonts | `Unity/Reflex/Assets/OpenFrontEnd/Resources/OpenFrontEnd/Fonts` | SIL Open Font License 1.1 |
| Capstone | `native/drafts/world_runtime/lib` (test oracle only) | BSD 3-Clause, see `CAPSTONE-LICENSE.TXT` |
| libmspack | `native/drafts/world_runtime/lib/libmspack` (test oracle only) | LGPL 2.1, see `COPYING.LIB` |
| FFmpeg (Windows player only, fetched at build time, not in the repository) | `builds/thirdparty/ffmpeg-win64` | LGPL 2.1+ (the LGPL shared build) |

MX vs ATV Reflex and its files belong to their rights holders. None of the game's files are in this repository.
