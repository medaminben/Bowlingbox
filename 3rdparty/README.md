# In-tree third-party code

| Directory | Project | Licence | How to regenerate |
|---|---|---|---|
| `glad/` | GLAD 2 loader, OpenGL 3.3 core, with `gladLoaderLoadGL()` | (WTFPL OR CC0-1.0) AND Apache-2.0 | `python -m glad --api='gl:core=3.3' --extensions='' --out-path=3rdparty/glad c --loader` |
| `stb/` | `stb_easy_font.h` and `stb_truetype.h` from https://github.com/nothings/stb | public domain / MIT | download the headers |
| `fonts/` | Rubik Medium (`Rubik-Medium.ttf`), embedded by Bowlingbox::Render for the HUD | SIL OFL 1.1 | download the static Medium TTF from https://github.com/googlefonts/rubik |
| `miniaudio/` | miniaudio 0.11.25 (`miniaudio.h`, `miniaudio.c`) from https://github.com/mackron/miniaudio | Unlicense (public domain) OR MIT-0 | download both files at tag `0.11.25` |
| `stb/stb_vorbis.c` | Ogg Vorbis decoder from https://github.com/nothings/stb, commit `2c980bb` — used only by the `sound_convert` dev tool, not shipped | public domain / MIT | download the file at that commit |
| `sounds/` | 13 CC0 samples embedded by Bowlingbox::Audio; provenance in `sounds/SOURCES.md` | CC0 1.0 | `sounds/SOURCES.md` lists the exact `sound_convert` command per file |

wxWidgets, Bullet3, GLM and GoogleTest are fetched at configure time (see `cmake/*Support.cmake`).
Full licence texts for all of them are in [`../licenses/`](../licenses/README.md).
