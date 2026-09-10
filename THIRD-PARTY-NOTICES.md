# Third-Party Notices

QuakeIIRTXHeadTracking bundles, statically links, or credits the third-party components
listed below. Each remains the property of its authors and is used under its own
licence. Where a licence requires the copyright notice, the conditions and the
disclaimer to accompany a binary distribution, the full text is reproduced here
verbatim, and this file ships at the root of every release ZIP we publish.

This repository redistributes no part of Quake II RTX. The engine conventions
it is written against, and where they come from, are recorded under "Quake II
and Q2RTX engine conventions" at the end of this file.

| Component | Version | Licence | How it ships |
|-----------|---------|---------|--------------|
| Ultimate ASI Loader | v9.7.4 | MIT | Bundled verbatim in the installer ZIP |
| injector | `3a384e8` (inside Ultimate ASI Loader v9.7.4) | Zlib | Compiled into the vendored dinput8.dll |
| miniz | 11.0.2 (inside Ultimate ASI Loader v9.7.4) | MIT | Compiled into the vendored dinput8.dll |
| MinHook | v1.3.4, one local change | BSD-2-Clause | Compiled into `QuakeIIRTXHeadTracking.asi` |
| cameraunlock-core | bd22895bb30ab7946d780b0af5782755e33e2cba | MIT | Compiled into `QuakeIIRTXHeadTracking.asi`, and its install scripts and game catalogue ship as source under `shared/` |
| OpenTrack | n/a | ISC | Not bundled; UDP protocol interoperability only |

---

## Ultimate ASI Loader

Vendored at `vendor/ultimate-asi-loader/`, shipped in the installer ZIP and used as the
install-time source. Taken from the upstream release asset untouched; the
upstream licence file ships beside it at `vendor/ultimate-asi-loader/LICENSE`.

`scripts/update-deps.ps1` rewrites the three unemphasised fields below by regex
matched on their exact line shape. Emphasising them stops the refresh silently.

- Version: `v9.7.4`
- Commit: `6b440669144c4a0bef5718ab155df160d231cd42`
- SHA-256: `fa266e3513d02c08a1b808f28c10538a489eaffaa4b0707f7cc1066e71b5afd7`
- **License:** MIT
- **Upstream:** https://github.com/ThirteenAG/Ultimate-ASI-Loader
- **Usage:** loads `QuakeIIRTXHeadTracking.asi` into the running game.
  `install.cmd` copies the vendored `dinput8.dll` into the Quake II RTX exe
  directory as `winmm.dll`.
- **Bundled:** yes. Shipped verbatim in the installer ZIP and extracted from
  `vendor/ultimate-asi-loader/` at install time; `install.cmd` never fetches it.

```
MIT License

Copyright (c) 2023 ThirteenAG

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

That `dinput8.dll` is a static binary and is not one component. The
`Ultimate-ASI-Loader-x64` target in `premake5.lua` at v9.7.4 compiles
`external/injector/minhook/src/**.c`,
`external/injector/utility/FunctionHookMinHook.cpp` and `external/miniz/miniz.c`
alongside the loader's own sources, so redistributing it redistributes MinHook,
injector and miniz as well, and each has its own section in this file.
MemoryModule, d3d8to9 and the minidx9 DirectX headers belong to the 32-bit
target only and are absent from this binary. The MinHook section covers the copy
inside the loader as well as any linked into the mod itself; the licence text is
the same.

---

## injector

Compiled into the vendored `dinput8.dll`. The loader's `FunctionHookMinHook`
wrapper, which the `Ultimate-ASI-Loader-x64` target compiles from
`external/injector/utility/FunctionHookMinHook.cpp`, and the MinHook submodule
that repository carries. Nothing in this repository calls or links it; it ships
only inside that binary.

- **Version:** commit `3a384e8d1b575c09383b0fab8bd92e34cb654949`, the submodule
  Ultimate ASI Loader v9.7.4 pins at `external/injector/`
- **License:** Zlib
- **Upstream:** https://github.com/ThirteenAG/injector
- **Usage:** the loader's `FunctionHookMinHook` wrapper. Nothing in this
  repository calls or links it.
- **Bundled:** yes, transitively. It ships only as machine code inside the
  vendored `dinput8.dll` in the installer ZIP.

The binary is unaltered upstream, so the "altered source versions" condition
below does not arise. It is reproduced whole regardless.

```
Copyright (C) 2012-2014 LINK/2012 <dma_2012@hotmail.com>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
```

---

## miniz

Compiled into the vendored `dinput8.dll`. Zip reading for the loader's
`LoadVirtualFilesFromZip` path, which the `Ultimate-ASI-Loader-x64` target
compiles from `external/miniz/miniz.c`. Nothing in this repository calls or
links it; it ships only inside that binary.

- **Version:** 11.0.2, as vendored at `external/miniz/` in Ultimate ASI Loader
  v9.7.4
- **License:** MIT
- **Upstream:** https://github.com/richgel999/miniz
- **Usage:** zip reading for the loader's `LoadVirtualFilesFromZip` path.
  Nothing in this repository calls or links it.
- **Bundled:** yes, transitively. It ships only as machine code inside the
  vendored `dinput8.dll` in the installer ZIP.

```
Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## MinHook

Source committed at `extern/minhook/` and compiled into `QuakeIIRTXHeadTracking.asi`. The
committed tree is the authoritative record of exactly what is built.

- **Version:** `v1.3.4`
- **Commit:** `c3fcafdc10146beb5919319d0683e44e3c30d537`
- **License:** BSD-2-Clause
- **Upstream:** https://github.com/TsudaKageyu/minhook
- **Usage:** installs the trampoline hook the mod places on the engine's
  crosshair draw function. It is the mod's only MinHook hook; the render hook
  is a function-pointer slot write and uses none of it.
- **Bundled:** yes, as source at `extern/minhook/`, compiled into
  `QuakeIIRTXHeadTracking.asi` and shipped in the installer ZIP as part of it.

MinHook carries two copyright holders: Tsuda Kageyu for MinHook itself, and
Vyacheslav Patkov for the Hacker Disassembler Engine that `extern/minhook/src/hde/` is built
from. Both notices appear below exactly as upstream ships them.

This copy is modified, in `extern/minhook/src/hook.c` and nowhere else: `MH_Initialize` uses
`GetProcessHeap()` rather than standing up a private heap with `HeapCreate`, and
`MH_Uninitialize` skips the matching `HeapDestroy`. Every other file under
`extern/minhook/` is byte for byte the v1.3.4 tag above, so a diff against that
tag both audits what gets compiled in and confirms that change is still the only
one. BSD-2-Clause permits it; it is recorded here so the attribution is not
mistaken for a claim of an unmodified copy.

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## cameraunlock-core

Git submodule at `cameraunlock-core/`, compiled into `QuakeIIRTXHeadTracking.asi`.
Parts of it also ship as source rather than as object code: the packager stages
the shared install and uninstall script bodies, the game-path detection module
and the game catalogue into `shared/` inside the installer ZIP, and `install.cmd`
calls into them. Our own code, MIT licensed, reproduced here so the notices are
complete.

- **Version:** commit `bd22895bb30ab7946d780b0af5782755e33e2cba`
- **License:** MIT
- **Upstream:** https://github.com/itsloopyo/cameraunlock-core
- **Usage:** the tracker receiver, the pose pipeline and the lean clamp policy,
  plus the shared install scripts and game catalogue `install.cmd` calls into.
- **Bundled:** yes. Compiled into `QuakeIIRTXHeadTracking.asi`, and staged as
  source under `shared/` in the installer ZIP.

```
MIT License

Copyright (c) 2026 itsloopyo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## OpenTrack

- **Version:** n/a. The wire format is consumed, not a pinned release.
- **License:** ISC
- **Upstream:** https://github.com/opentrack/opentrack
- **Usage:** the mod implements OpenTrack's UDP pose datagram layout so that
  OpenTrack and compatible trackers can drive it.
- **Bundled:** no. Not linked either.

No OpenTrack code, headers or binaries are copied, linked or redistributed, so
its licence triggers no notice obligation here. It is credited because the wire
format is its work.

---

## Statically linked into QuakeIIRTXHeadTracking.asi

- **MinHook** - Tsuda Kageyu. SPDX: BSD-2-Clause.
  https://github.com/TsudaKageyu/minhook
- **cameraunlock-core** - itsloopyo. SPDX: MIT.
  https://github.com/itsloopyo/cameraunlock-core

---

## Quake II RTX

Quake II RTX and all related names, logos, characters and marks are trademarks
of their respective owners. They are used here only to identify the game this
mod applies to, which is nominative use and not a claim of any right in them.
This project is an unofficial, fan-made modification. It is not affiliated
with, endorsed by, or sponsored by the game's developers, its publishers, its
engine vendor, or any other rights holder. It redistributes no game code, no
game assets and no proprietary DLLs, and it requires a legitimately purchased
copy of the game.

## Quake II and Q2RTX engine conventions

id Software's Quake II source and NVIDIA's Quake II RTX, which is derived from
it by way of Q2PRO, are both published under the GNU General Public License,
version 2. That published source is where the engine vocabulary this mod is
written against comes from, and naming it is what makes the boundary auditable:

- **Structure names and member names.** `refdef_t` (`vieworg`, `viewangles`,
  `fov_x`, `fov_y`), `trace_t` (`allsolid`, `startsolid`, `fraction`, `endpos`,
  `plane`, `surface`, `contents`, `ent`) and its `cplane_t` normal, `cvar_t`,
  `bsp_t`, `client_state_t` (`cl.fov_x`, `cl.maxclients`), `client_static_t`
  (`cls.state`, `cls.key_dest`), the `r_config` framebuffer pair and the screen
  state's `hud_width` / `hud_height`. They appear in
  `src/core/build_profiles.h`, `src/hooks/render_hook.cpp`,
  `src/hooks/crosshair_hook.cpp`, `src/engine/fov_zoom.h`, `src/engine/cvar.h`,
  `src/engine/lean_trace.{h,cpp}` and `src/engine/game_state.cpp`.
- **Function names, as the things this mod hooks or calls by address.**
  `R_RenderFrame` and `R_Init`, `CL_Trace`, `SCR_DrawCrosshair`, and - named in
  comments only, to say where a value comes from or which code moves it -
  `V_RenderView`, `V_CalcFov`, `ClientEndServerFrame` and
  `ClientUserinfoChanged`.
- **Enumerator and constant names, and the values behind them.** `ca_active` in
  `connstate_t`, `KEY_GAME` in `keydest_t`, and the `MASK_SOLID` / `MASK_SHOT`
  content bits in `src/engine/lean_trace.h`.
- **Console variable names**, as user-facing settings the mod reads or refers
  to: `fov`, `ch_x`, `ch_y`, `cl_adjustfov`, `cl_rollangle`.
- **The `AngleVectors` rotation convention** that `src/quake_math.cpp`
  implements, the pitch/yaw/roll ordering of a Quake angle triple, and the
  world axes (x forward, y left, z up) and unit scale that go with them.

The per-build addresses and structure offsets in `src/core/steam_offsets.cpp`
are read from the symbols the shipped `q2rtx.exe` and `q2rtx.pdb` carry. They
are numbers measured from a legitimately owned installation.

That is the whole extent of it: names, numeric constants and layouts, which is
what one program needs to talk to another. No GPL-licensed source is copied
into, generated into, compiled into or linked by this repository, at build time
or at any other time. `src/quake_math.cpp` is an independent implementation of
the convention named above, not a copy of any Quake II source file, and no
decompiled or disassembled game code is stored here. The mod is built without
any part of the game present, and reaches the engine only through the addresses
above, inside the player's own process, at runtime. This project is distributed
under the MIT licence and links only the MIT, BSD-2-Clause and Zlib components
listed above.
