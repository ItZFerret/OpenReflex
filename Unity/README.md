# Unity/Reflex

The Unity 6000.6.1f1 project of OPEN REFLEX: it draws the game the C core (`../core`) runs, plays its audio, reads input and
runs the menus. It reads the player's own MX vs ATV Reflex files at run time; no game data is part of the project.

- `Assets/Scripts`: the client. `OpenReflex/` binds the core (`librxcore`), `OriginalWorld/` builds and draws a running event,
  `OpenFrontEnd/` and `FrontEnd/` the menus, `Content/` the readers of the game's formats, the importer and the mod loaders,
  `Rendering/`, `Hud/`, `Options/`, `Practice/`, `Replay/`, `Platform/`.
- `Assets/Editor`: build scripts (`Build/OpenReflexBuild.cs`) and editor tests.
- `Assets/Scenes`: `FrontEnd` (the menus, build index 0) and `Reflex` (a running event) are the player's scenes; the others are
  development scenes.

Build a player with `../core/tools/build_mac_player.sh` or `../core/tools/build_windows_player.sh` (see the top-level README);
they stage a copy of this project, so an open editor is not disturbed. `../tools/unity_compile.sh` checks that the scripts
compile without opening Unity.
