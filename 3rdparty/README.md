# In-tree third-party code

| Directory | Project | Licence | How to regenerate |
|---|---|---|---|
| `glad/` | GLAD 2 loader, OpenGL 3.3 core, with `gladLoaderLoadGL()` | (WTFPL OR CC0-1.0) AND Apache-2.0 | `python -m glad --api='gl:core=3.3' --extensions='' --out-path=3rdparty/glad c --loader` |
| `stb/` | `stb_easy_font.h` from https://github.com/nothings/stb | public domain / MIT | download the header |

wxWidgets, Bullet3, GLM and GoogleTest are fetched at configure time (see `cmake/*Support.cmake`).
Full licence texts for all of them are in [`../licenses/`](../licenses/README.md).
