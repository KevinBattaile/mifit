#!/bin/bash

## Force Qt6 to use Desktop OpenGL instead of GLES2
#export QT_OPENGL=desktop
#export QSG_RHI_BACKEND=opengl
#export QT_XCB_GL_INTEGRATION=xcb_glx

## Lots of logging output
#export QSG_INFO=1
#export QT_DEBUG_PLUGINS=1

# NEW: Force the AMD Mesa driver to bypass DRI3 and use DRI2
# This prevents the xcb_wait_for_special_event deadlock!
#export LIBGL_DRI3_DISABLE=1

# Optional but highly recommended: Force Mesa to provide a Compatibility Profile natively
#export MESA_GL_VERSION_OVERRIDE=4.5COMPAT

# Move to build directory and launch
cd "$(dirname "$0")/build"
#gdb -ex run --args ./MIFit "$@"
./MIFit "$@"
