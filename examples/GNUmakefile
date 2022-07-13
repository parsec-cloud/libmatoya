name = 0-minimal

.PHONY: clean app

all: clean app

#############
### APPLE ###
#############

ifdef apple

ifndef ARCH
ARCH = $(shell uname -m)
endif

BIN   = example
OBJ   = $(name).o
LIBS  = ../bin/macosx/$(ARCH)/libmatoya.a

CFLAGS  = -I../src -Igen -Iparsec -O0 -g
LDFLAGS = \
	-framework OpenGL \
	-framework AppKit \
	-framework Metal \
	-framework IOKit \
	-framework CoreVideo \
	-framework Carbon \
	-framework QuartzCore \
	-framework Security \
	-framework WebKit \
	 -O0 -g

clean:
	rm -f $(BIN) $(OBJ)

app: $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(LIBS) $(LDFLAGS) -arch $(ARCH)

%.o: %.c
	$(CC) -c $< $(CFLAGS) -arch $(ARCH)

endif

#############
### LINUX ###
#############

ifdef linux

BIN   = example
OBJ   = $(name).o
LIBS  = ../bin/linux/x86_64/libmatoya.a

CFLAGS  = -I../src -Igen
LDFLAGS = $(shell pkg-config --libs x11 gtk+-3.0 webkit2gtk-4.0) -lm -ldl -lpthread

clean:
	rm -f $(BIN) $(OBJ)

app: $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(LIBS) $(LDFLAGS)

endif

############
### WASM ###
############

ifdef wasm

WASI_SDK = $(HOME)/wasi-sdk-16.0

CC = $(WASI_SDK)/bin/clang --sysroot=$(WASI_SDK)/share/wasi-sysroot

BIN   = example.wasm
OBJ   = $(name).o
LIBS  = ../bin/web/wasm32/libmatoya.a

CFLAGS  = -I../src -Igen
LDFLAGS = -Wl,--allow-undefined -Wl,--export-table

clean:
	rm -f web/$(BIN) $(OBJ)

app: $(OBJ)
	@cp ../src/unix/web/matoya.js web/
	$(CC) -o web/$(BIN) $(OBJ) $(LIBS) $(LDFLAGS)

endif

###############
### ANDROID ###
###############

ifdef android

ANDROID_PROJECT        = android
ANDROID_PACKAGE        = group.matoya
ANDROID_APPLICATION_ID = group.matoya.debug

ANDROID_NDK             = $(HOME)/android-ndk-r23
export ANDROID_HOME     = $(HOME)/android-home
export ANDROID_SDK_ROOT = $(ANDROID_HOME)/cmdline-tools

gradle:
	@java \
		-classpath $(ANDROID_PROJECT)/gradle/wrapper/gradle-wrapper.jar \
		org.gradle.wrapper.GradleWrapperMain \
		-p $(ANDROID_PROJECT) \
		$(CMD)

ndk:
	@mkdir -p $(ANDROID_PROJECT)/app/libs
	@$(ANDROID_NDK)/ndk-build $(CMD) -j4 \
		name=$(name) \
		APP_BUILD_SCRIPT=Android.mk \
		APP_PLATFORM=android-26 \
		NDK_PROJECT_PATH=. \
		NDK_OUT=$(ANDROID_PROJECT)/build \
		NDK_LIBS_OUT=$(ANDROID_PROJECT)/app/libs \
		--no-print-directory \
		| grep -v 'fcntl(): Operation not supported'	

clean:
	@make --no-print-directory ndk CMD=clean
	@make --no-print-directory gradle CMD=clean

app:
	@mkdir -p android/app/src/main/java/group/matoya/lib
	@cp ../src/unix/linux/android/Matoya.java android/app/src/main/java/group/matoya/lib/
	@make --no-print-directory ndk
	@make --no-print-directory gradle CMD=build

run:
	@make --no-print-directory gradle CMD=installDebug
	@$(ANDROID_HOME)/platform-tools/adb shell am force-stop $(ANDROID_APPLICATION_ID)
	@$(ANDROID_HOME)/platform-tools/adb logcat -c
	@$(ANDROID_HOME)/platform-tools/adb shell am start -n $(ANDROID_APPLICATION_ID)/$(ANDROID_PACKAGE).MainActivity
	@$(ANDROID_HOME)/platform-tools/adb logcat $(ANDROID_APPLICATION_ID) | grep ' E \|MTY'

endif
