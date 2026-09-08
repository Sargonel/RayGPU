CC = clang
DAWN = ../dawn
DAWN_BUILD = build/dawn-release

CFLAGS = -std=c17 -O2 -Wall -Wextra
NATIVE_FLAGS = -I$(DAWN)/include -I$(DAWN_BUILD)/gen/include
NATIVE_LIBS = -L$(DAWN_BUILD)/src/dawn/native -lwebgpu_dawn -lDXGuid -lKernel32 -lOneCore -luser32 -lgdi32 -lwinmm
WEB_FLAGS = --target=wasm32 -ffreestanding -nostdlib -Wl,--no-entry -Wl,--export=main -Wl,--export-memory -Wl,-z,stack-size=1048576 -Wl,--initial-memory=67108864 -Wl,--max-memory=2147483648

.PHONY: native web run server clean

native:
	$(CC) $(CFLAGS) $(NATIVE_FLAGS) main.c $(NATIVE_LIBS) -o main.exe
	powershell -NoProfile -Command "Copy-Item -LiteralPath (Join-Path $$env:SystemRoot 'System32/d3dcompiler_47.dll') -Destination 'd3dcompiler_47.dll' -Force"

web:
	cmd /C "if not exist build\web\assets mkdir build\web\assets"
	$(CC) $(CFLAGS) $(WEB_FLAGS) main.c -o build/web/main.wasm
	powershell -NoProfile -Command "Copy-Item -LiteralPath 'shell.html' -Destination 'build/web/index.html' -Force"
	powershell -NoProfile -Command "Copy-Item -LiteralPath 'raygpu.js' -Destination 'build/web/raygpu.js' -Force"
	powershell -NoProfile -Command "Copy-Item -Path 'assets/*' -Destination 'build/web/assets' -Recurse -Force"

run: native
	.\main.exe

server: web
	powershell -NoProfile -ExecutionPolicy Bypass -File serve.ps1

clean:
	powershell -NoProfile -Command "Remove-Item -LiteralPath 'main.exe','d3dcompiler_47.dll','build/web' -Recurse -Force -ErrorAction SilentlyContinue"
