name = 0-minimal

OBJS = $(name).obj

CFLAGS = \
	/MT \
	/MP \
	/DWIN32_LEAN_AND_MEAN \
	/DUNICODE \
	/Igen \
	/I../src \
	/wd4100 \
	/volatile:iso \
	/nologo

MANIFEST = \
	/manifest:embed \
	/manifestinput:embed.manifest

LD_FLAGS = \
	$(MANIFEST) \
	/nodefaultlib \
	/subsystem:console \
	/nologo

!IFDEF DEBUG
CFLAGS = $(CFLAGS) /Ob0 /Zi
LD_FLAGS = $(LD_FLAGS) /DEBUG
!ELSE
CFLAGS = $(CFLAGS) /O2 /GS- /Gw
!ENDIF

LIBS = \
	libvcruntime.lib \
	libucrt.lib \
	libcmt.lib \
	kernel32.lib

MTY_LIBS = \
	shlwapi.lib \
	shell32.lib \
	user32.lib \
	userenv.lib \
	ws2_32.lib \
	imm32.lib \
	advapi32.lib \
	dxguid.lib \
	winmm.lib \
	gdi32.lib \
	bcrypt.lib \
	secur32.lib \
	crypt32.lib \
	opengl32.lib \
	xinput9_1_0.lib \
	windowscodecs.lib \
	d3d9.lib \
	d3d11.lib \
	hid.lib \
	ole32.lib \
	winhttp.lib \
	uuid.lib

STATIC_LIBS = \
	..\bin\windows\%Platform%\matoya.lib \
	windows\%Platform%\WebView2LoaderStatic.lib

all: clean clear $(OBJS)
	link /out:example.exe *.obj $(LD_FLAGS) $(LIBS) $(MTY_LIBS) $(STATIC_LIBS)

clean:
	del *.obj
	del *.exe

clear:
	cls
