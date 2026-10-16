// Throwaway app: run once on the Apple TV so Xcode creates the signing certificate
// and a provisioning profile for tv.parsec.mty-spike that covers the device.
import SwiftUI

@main
struct SigningHelperApp: App {
	var body: some Scene {
		WindowGroup {
			Text("Signing helper ran on this Apple TV.\nYou can close it.")
				.multilineTextAlignment(.center)
				.font(.title)
		}
	}
}
