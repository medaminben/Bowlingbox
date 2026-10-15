# Third-party licences

Bowlingbox itself is MIT licensed (see [`../LICENSE`](../LICENSE)). This folder lists
every third-party component the build uses, with the full licence text of each
one copied from that project's own source.

## In-tree (`3rdparty/`)

| Component | Version | Used by | Licence | Text |
|---|---|---|---|---|
| [glad 2](https://github.com/Dav1dde/glad) generated loader | OpenGL 3.3 core | Render | (WTFPL OR CC0-1.0) AND Apache-2.0; `khrplatform.h`: Khronos MIT-style | [glad.txt](glad.txt) |
| [stb_easy_font](https://github.com/nothings/stb) | 1.1 | Render (overlay text) | MIT OR Unlicense (public domain) | [stb.txt](stb.txt) |

## Fetched at configure time (`cmake/*Support.cmake`)

| Component | Version | Used by | Licence | Text |
|---|---|---|---|---|
| [Bullet3](https://github.com/bulletphysics/bullet3) | 3.25 | Alley (statically linked) | Zlib | [bullet3.txt](bullet3.txt) |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | Render (header-only) | MIT OR Happy Bunny License | [glm.txt](glm.txt) |
| [GoogleTest](https://github.com/google/googletest) | 1.15.2 | tests only, not shipped | BSD-3-Clause | [googletest.txt](googletest.txt) |
| [wxWidgets](https://www.wxwidgets.org) | ≥ 3.2 (3.2.8 when built from source) | `BowlingGL`, `BowlingDisplay` | wxWindows Library Licence 3.1 (LGPL-2.0+ with a binary exception) | [wxWidgets.txt](wxWidgets.txt) |

### Built into wxWidgets when it is compiled from source

`cmake/WxSupport.cmake` uses an installed wxWidgets if there is one. Otherwise it
builds 3.2.8 statically with these bundled libraries, so they end up in the
application binaries:

| Component | Version | Licence | Text |
|---|---|---|---|
| libpng | 1.6.37 | PNG Reference Library License v2 (libpng-2.0) | [libpng.txt](libpng.txt) |
| libjpeg (IJG) | 9d | IJG | [libjpeg.txt](libjpeg.txt) |
| Expat | 2.4.8 | MIT | [expat.txt](expat.txt) |
| PCRE2 | 10.37 | BSD-3-Clause (with PCRE2 exception) | [pcre2.txt](pcre2.txt) |
| NanoSVG | bundled | Zlib | [nanosvg.txt](nanosvg.txt) |

## System libraries (linked, not redistributed)

These come from the host system, under the licences of their distribution
packages. They are listed here for completeness:

| Component | When | Licence |
|---|---|---|
| zlib | wxWidgets source build (`wxUSE_ZLIB=sys`) | Zlib |
| OpenGL (libGL / libOpenGL / GLX) | GL and wx applications | vendor driver licence (Mesa: MIT) |
| X11, GTK 3 and dependencies | wxWidgets on Linux | MIT / LGPL-2.1+ |

## Windows installer

The Windows build (`cmake/windows-installer.cmake`) differs in three ways:

- wxWidgets is built from source with `wxUSE_LIBJPEG`, `wxUSE_EXPAT` and
  `wxUSE_REGEX` off and its own zlib, so libjpeg, Expat and PCRE2 are not in the
  binary. libpng stays (wxMSW requires it) and needs no binary notice.
- The GCC runtime is linked statically (`BOWLINGBOX_STATIC_RUNTIME`), which adds
  the mingw-w64 runtime: [mingw-w64.txt](mingw-w64.txt).
- GoogleTest is built statically on every platform; it is still tests-only.

The installer ships one file, `installer/THIRD_PARTY_NOTICES.txt`, holding the
full texts of GLM, glad (with Apache-2.0 and `khrplatform.h`) and the mingw-w64
runtime, the components whose licences require a notice in binary copies. When
one of those licence files changes, regenerate the notices file with the
commands in `docs/windows-installer.md`.

## Updating

When you bump a version in `cmake/*Support.cmake` or regenerate `3rdparty/`,
update the table above and copy the new licence file from the fetched source
(`build/_deps/<name>-src/`) into this folder.
