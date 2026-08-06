#include "licenses.h"

namespace
{
/// Attribution for the vendored third-party code. Both are MIT and both
/// require the notice to travel with the binary, which is what this screen is
/// for.
constexpr char const *TEXT =
    "3FC\n"
    "FTP client for Nintendo 3DS.\n"
    "\n"
    "----------------------------------------\n"
    "Dear ImGui 1.91.8\n"
    "Copyright (c) 2014-2025 Omar Cornut\n"
    "MIT License\n"
    "\n"
    "Permission is hereby granted, free of charge, to any person obtaining a "
    "copy of this software and associated documentation files (the "
    "\"Software\"), to deal in the Software without restriction, including "
    "without limitation the rights to use, copy, modify, merge, publish, "
    "distribute, sublicense, and/or sell copies of the Software, and to permit "
    "persons to whom the Software is furnished to do so, subject to the "
    "following conditions:\n"
    "\n"
    "The above copyright notice and this permission notice shall be included "
    "in all copies or substantial portions of the Software.\n"
    "\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS "
    "OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF "
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN "
    "NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, "
    "DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR "
    "OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE "
    "USE OR OTHER DEALINGS IN THE SOFTWARE.\n"
    "\n"
    "----------------------------------------\n"
    "ImGui 3DS backend (imgui_citro3d, imgui_ctru, vshader.v.pica)\n"
    "Copyright (C) 2020, 2024 Michael Theall\n"
    "MIT License\n"
    "\n"
    "Taken from ftpd (https://github.com/mtheall/ftpd). The ftpd repository as "
    "a whole is GPL-3.0, but these files carry their own MIT notice.\n"
    "\n"
    "Permission is hereby granted, free of charge, to any person obtaining a "
    "copy of this software and associated documentation files (the "
    "\"Software\"), to deal in the Software without restriction, including "
    "without limitation the rights to use, copy, modify, merge, publish, "
    "distribute, sublicense, and/or sell copies of the Software, and to permit "
    "persons to whom the Software is furnished to do so, subject to the "
    "following conditions:\n"
    "\n"
    "The above copyright notice and this permission notice shall be included "
    "in all copies or substantial portions of the Software.\n"
    "\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS "
    "OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF "
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN "
    "NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, "
    "DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR "
    "OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE "
    "USE OR OTHER DEALINGS IN THE SOFTWARE.\n"
    "\n"
    "----------------------------------------\n"
    "devkitARM, libctru and citro3d are distributed under their own terms; see "
    "https://devkitpro.org.\n";
}

char const *licenses::text ()
{
	return TEXT;
}
