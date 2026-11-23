MIFit is a cross-platform interactive graphics application for molecular modeling, fitting, and
refinement of protein structures from x-ray crystallography.

MIFit has the following features:
  * Easy to navigate and manipulate multiple structures and maps.
  * Improvements in graphics rendering performance.
  * Customizable jobs menu for interfacing with MIExpert or your own scripts and automated systems.
  * Developed with Qt 6 (http://www.qt.io).
  * Cross platform – supports Linux and MacOS.  Building on Windows should be possible.

![Screenshot](https://raw.githubusercontent.com/mifit/wiki/master/images/image95.png)

## News

### November 2026

Modernized release (v2026.11): ported MIFit to Qt 6 and CMake/Ninja, added gemmi-back mmCIF support,
resored the local-socket scripting IPC server, added Conda/RPATH zero-activation deployment and
added native MacOS Apple Silicon support. Pair with the matching v2026.11 release of MIExpert (ported
to Python 3 and PyQt6).

### March 2015

Project moved to GitHub because Google Code is shutting down.

### October 2010
MIFit 2010.10 is available for download. See [Releases](https://github.com/mifit/mifit/releases).
You may build MIFit yourself from the source distribution (see the enclosed README.txt), or
download the Windows installer or Linux binaries archive.

MIExpert is now distributed as a separate package. Although it can be used separately, MIExpert
is designed to be used with MIFit. To use MIExpert with MIFit, run MIFit and open the
MIExpert/setupJobMenu.py file. The setupJobMenu.py script will configure the Job menu in MIFit.
Note: MIExpert scripts require [PyQt](http://www.riverbankcomputing.co.uk/software/pyqt/intro)
in order to run from MIFit.


