# In-tree third-party code

| Directory | Project | Licence | How to regenerate |
|---|---|---|---|
| `glad/` | GLAD 2 loader, OpenGL 3.3 core, with `gladLoaderLoadGL()` | MIT / Apache-2.0 (generator), public domain (output) | `python -m glad --api='gl:core=3.3' --extensions='' --out-path=3rdparty/glad c --loader` |
| `stb/` | `stb_easy_font.h` from https://github.com/nothings/stb | public domain / MIT | download the header |

wxWidgets, Bullet3, GLM and GoogleTest are fetched at configure time (see `cmake/*Support.cmake`).
