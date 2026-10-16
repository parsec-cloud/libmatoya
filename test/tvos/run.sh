#!/bin/zsh
# Build the tvOS spike app against libmatoya and run it in the simulator or on an Apple TV.
#
# Usage: test/tvos/run.sh sim|device [seconds-to-capture]
#
# Device signing is detected automatically. Override with:
#   TVOS_DEVICE     UDID of the Apple TV (default: first paired physical Apple TV)
#   TVOS_IDENTITY   codesigning identity hash (default: first "Apple Development" identity)
#   TVOS_PROFILE    path to a .mobileprovision (default: newest tvOS profile for TVOS_BUNDLE_ID
#                   that includes the device)
#   TVOS_BUNDLE_ID  (default: tv.parsec.mty-spike)
#   TVOS_SIM        simulator device name (default: Apple TV 4K (3rd generation))
set -eu
HERE=${0:A:h}
ROOT=${HERE:h:h}
MODE=${1:-sim}
SECS=${2:-60}
BUNDLE_ID=${TVOS_BUNDLE_ID:-tv.parsec.mty-spike}
OUT=$HERE/build/$MODE
APP=$OUT/MTYSpike.app

case $MODE in
	sim)
		TARGET=appletvsimulator
		CLANG_TARGET=arm64-apple-tvos14.0-simulator
		PLATFORM=AppleTVSimulator
		;;
	device)
		TARGET=appletvos
		CLANG_TARGET=arm64-apple-tvos13.0
		PLATFORM=AppleTVOS
		;;
	*)
		echo "usage: $0 sim|device [seconds]" >&2
		exit 1
		;;
esac

rm -rf $OUT && mkdir -p $APP

# libmatoya objects live next to their sources and are shared between targets, so
# remove them before building. Unlike 'make clean-build', this leaves bin/ alone.
echo "== building libmatoya ($TARGET)"
printf 'include GNUmakefile\nclean-objs:\n\t@rm -f $(OBJS)\n' > $OUT/clean.mk
make -s -C $ROOT -f $OUT/clean.mk TARGET=$TARGET ARCH=arm64 clean-objs
make -s -C $ROOT TARGET=$TARGET ARCH=arm64 src/unix/apple/gfx/shaders/quad.h src/unix/apple/gfx/shaders/ui.h
make -s -C $ROOT TARGET=$TARGET ARCH=arm64 objs -j8 >/dev/null
LIB=$ROOT/bin/$TARGET/arm64/libmatoya.a

# The app's main() becomes mty_main; libmatoya provides the real main()
echo "== building app"
SDK=$(xcrun --sdk $TARGET --show-sdk-path)
xcrun clang --target=$CLANG_TARGET -isysroot $SDK \
	-I$ROOT/src -O2 -Wall -Wextra -Wno-unused-parameter \
	-Dmain=mty_main \
	-o $APP/MTYSpike $HERE/spike.c $HERE/diag.m -fobjc-arc \
	$LIB \
	-framework UIKit -framework Foundation -framework Metal -framework QuartzCore \
	-framework GameController -framework AudioToolbox -framework CoreGraphics \
	-framework ImageIO -framework Security -framework CoreServices

cat > $APP/Info.plist <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
	<key>CFBundleExecutable</key><string>MTYSpike</string>
	<key>CFBundleName</key><string>MTY Spike</string>
	<key>CFBundlePackageType</key><string>APPL</string>
	<key>CFBundleShortVersionString</key><string>1.0</string>
	<key>CFBundleVersion</key><string>1</string>
	<key>CFBundleSupportedPlatforms</key><array><string>$PLATFORM</string></array>
	<key>MinimumOSVersion</key><string>13.0</string>
	<key>UIDeviceFamily</key><array><integer>3</integer></array>
	<key>UIRequiredDeviceCapabilities</key><array><string>arm64</string></array>
	<key>GCSupportsControllerUserInteraction</key><true/>
	<key>GCSupportedGameControllers</key>
	<array>
		<dict><key>ProfileName</key><string>ExtendedGamepad</string></dict>
	</array>
	<key>UIApplicationSceneManifest</key>
	<dict>
		<key>UIApplicationSupportsMultipleScenes</key><false/>
	</dict>
</dict>
</plist>
EOF

if [[ $MODE == sim ]]; then
	codesign -s - --force $APP >/dev/null 2>&1

	SIM=${TVOS_SIM:-Apple TV 4K (3rd generation)}
	UDID=$(xcrun simctl list devices available | grep -F "$SIM (" | head -1 | grep -oE '[0-9A-F-]{36}')
	xcrun simctl boot $UDID 2>/dev/null || true
	xcrun simctl bootstatus $UDID -b >/dev/null
	xcrun simctl install $UDID $APP

	echo "== launching on $SIM ($UDID), capturing ${SECS}s of console"
	xcrun simctl launch --console-pty --terminate-running-process $UDID $BUNDLE_ID > $OUT/console.log 2>&1 &
	LPID=$!
	sleep $SECS
	xcrun simctl io $UDID screenshot $OUT/screenshot.png >/dev/null 2>&1 || true
	kill $LPID 2>/dev/null || true

else
	DEVICE=${TVOS_DEVICE:-$(xcrun devicectl list devices 2>/dev/null |
		awk '/Apple TV/ && /physical/ { for (i = 1; i <= NF; i++) if ($i ~ /^[0-9A-F]{8}-/) { print $i; exit } }')}
	if [[ -z $DEVICE ]]; then
		echo "No paired Apple TV found, see test/tvos/README.md" >&2
		exit 1
	fi

	IDENTITY=${TVOS_IDENTITY:-$(security find-identity -v -p codesigning | awk '/Apple Development/ { print $2; exit }')}
	if [[ -z $IDENTITY ]]; then
		echo "No Apple Development signing identity found, see test/tvos/README.md" >&2
		exit 1
	fi

	# Newest unexpired tvOS profile for the bundle ID that includes this device
	PROFILE=${TVOS_PROFILE:-$(python3 - $BUNDLE_ID $DEVICE <<'PYEOF'
import glob, os, plistlib, subprocess, sys
from datetime import datetime, timezone
bundle_id, device = sys.argv[1], sys.argv[2]
best = None
for path in glob.glob(os.path.expanduser('~/Library/Developer/Xcode/UserData/Provisioning Profiles/*.mobileprovision')):
	raw = subprocess.run(['security', 'cms', '-D', '-i', path], capture_output=True).stdout
	try:
		p = plistlib.loads(raw)
	except Exception:
		continue
	app_id = p.get('Entitlements', {}).get('application-identifier', '')
	expires = p['ExpirationDate'].replace(tzinfo=timezone.utc)
	if ('tvOS' in p.get('Platform', []) and app_id.endswith('.' + bundle_id)
			and device in p.get('ProvisionedDevices', []) and expires > datetime.now(timezone.utc)):
		if not best or expires > best[0]:
			best = (expires, path)
print(best[1] if best else '')
PYEOF
)}
	if [[ -z $PROFILE ]]; then
		echo "No valid tvOS provisioning profile for $BUNDLE_ID on $DEVICE, see test/tvos/README.md" >&2
		exit 1
	fi

	# Sign with exactly the entitlements the provisioning profile grants
	cp "$PROFILE" $APP/embedded.mobileprovision
	security cms -D -i "$PROFILE" > $OUT/profile.plist
	/usr/libexec/PlistBuddy -x -c 'Print :Entitlements' $OUT/profile.plist > $OUT/entitlements.plist
	codesign --force --timestamp=none --sign $IDENTITY --entitlements $OUT/entitlements.plist $APP
	codesign --verify --strict $APP

	echo "== installing on $DEVICE"
	xcrun devicectl device install app --device $DEVICE $APP --quiet

	echo "== launching, capturing ${SECS}s of console"
	xcrun devicectl device process launch --device $DEVICE --console --terminate-existing $BUNDLE_ID > $OUT/console.log 2>&1 &
	LPID=$!
	sleep $SECS
	kill $LPID 2>/dev/null || true
fi

echo "== console ($OUT/console.log)"
cat $OUT/console.log
