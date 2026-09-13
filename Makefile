CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -O2 -Iinclude
# std::thread (ton.sys/ton.time sleep helpers) needs pthread on Linux/macOS;
# harmless to pass on other platforms where it's simply unused.
LDFLAGS = -pthread
SRC = src/Lexer.cpp src/Parser.cpp src/Interpreter.cpp src/Debugger.cpp \
      src/LocalServer.cpp src/HttpClient.cpp src/JsonParser.cpp src/main.cpp
OUT = ton618
DIST = dist

# Cross-compilers used to produce every precompiled binary released on
# GitHub (see .github/workflows/release.yml and scripts/rebuild.sh for the
# same builds done in CI and locally). End users never compile anything
# themselves: no clang, no g++, no make, and — for Android/Termux — no
# Android NDK either. The Termux binary is cross-compiled against musl
# instead of the NDK's bionic target (typically via Zig, which bundles musl
# libc); statically linked, it runs unmodified under Termux with zero
# packages installed on-device.
MINGW_CXX ?= x86_64-w64-mingw32-g++
MINGW_STRIP ?= x86_64-w64-mingw32-strip
MUSL_AARCH64_CXX ?= zig c++ -target aarch64-linux-musl
MUSL_AARCH64_STRIP ?= scripts/zig-strip.sh

all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

# --- Precompiled release binaries (dist/) -----------------------------------
#
# These are what install.sh / install.ps1 actually ship: end users only ever
# download one of these, they never build from source.

linux: $(DIST)/ton618-linux-x64

$(DIST)/ton618-linux-x64: $(SRC)
	mkdir -p $(DIST)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@ $(LDFLAGS)
	strip $@
	chmod +x $@

windows: $(DIST)/ton618-windows-x64.exe

$(DIST)/ton618-windows-x64.exe: $(SRC)
	mkdir -p $(DIST)
	$(MINGW_CXX) -std=c++17 -Wall -O2 -D_USE_MATH_DEFINES -Iinclude -static $(SRC) -o $@ -lws2_32
	$(MINGW_STRIP) $@

android: $(DIST)/ton618-android-arm64

$(DIST)/ton618-android-arm64: $(SRC)
	mkdir -p $(DIST)
	$(MUSL_AARCH64_CXX) $(CXXFLAGS) -static $(SRC) -o $@ $(LDFLAGS)
	$(MUSL_AARCH64_STRIP) $@
	chmod +x $@

dist: linux windows android

clean:
	rm -f $(OUT)

distclean: clean
	rm -f $(DIST)/ton618-linux-x64 $(DIST)/ton618-windows-x64.exe $(DIST)/ton618-android-arm64

# Regenerates exemples/documentation.html by embedding the current
# DOCUMENTATION.md between the two page templates in scripts/doc_templates/.
# The page renders that embedded markdown directly (no fetch, no server
# required) — run this after editing DOCUMENTATION.md so the page doesn't
# drift out of sync with it.
docs:
	cat scripts/doc_templates/head.html DOCUMENTATION.md scripts/doc_templates/tail.html > exemples/documentation.html

.PHONY: all linux windows android dist clean distclean docs
