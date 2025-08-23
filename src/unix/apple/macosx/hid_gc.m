// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>
#import <IOKit/hid/IOHIDDevice.h>

// Structure to hold rumble context for a controller
@interface MTYRumbleMotor : NSObject
@property(nonatomic, strong) CHHapticEngine *engine API_AVAILABLE(macos(11.0));
@property(nonatomic, strong) id<CHHapticPatternPlayer> player API_AVAILABLE(macos(11.0));
@property(nonatomic) BOOL active;
@end

@implementation MTYRumbleMotor

- (void)cleanup {
    @autoreleasepool {
        if (@available(macOS 11.0, *)) {
            if (self.player != nil) {
                [self.player cancelAndReturnError:nil];
                self.player = nil;
            }
            if (self.engine != nil) {
                [self.engine stopWithCompletionHandler:nil];
                self.engine = nil;
            }
        }
    }
}

- (BOOL)setIntensity:(float)intensity {
    @autoreleasepool {
        if (@available(macOS 11.0, *)) {
            NSError *error = nil;
            
            if (self.engine == nil) {
                NSLog(@"Haptics engine was stopped");
                return NO;
            }
            
            // Stop rumble if intensity is 0
            if (intensity == 0.0f) {
                if (self.player && self.active) {
                    [self.player stopAtTime:0 error:&error];
                }
                self.active = NO;
                return YES;
            }
            
            // Create player if needed
            if (self.player == nil) {
                CHHapticEventParameter *eventParam = [[CHHapticEventParameter alloc] 
                    initWithParameterID:CHHapticEventParameterIDHapticIntensity value:1.0f];
                CHHapticEvent *event = [[CHHapticEvent alloc] 
                    initWithEventType:CHHapticEventTypeHapticContinuous 
                    parameters:@[eventParam] 
                    relativeTime:0 
                    duration:GCHapticDurationInfinite];
                CHHapticPattern *pattern = [[CHHapticPattern alloc] 
                    initWithEvents:@[event] 
                    parameters:@[] 
                    error:&error];
                
                if (error != nil) {
                    NSLog(@"Couldn't create haptic pattern: %@", error.localizedDescription);
                    return NO;
                }
                
                self.player = [self.engine createPlayerWithPattern:pattern error:&error];
                if (error != nil) {
                    NSLog(@"Couldn't create haptic player: %@", error.localizedDescription);
                    return NO;
                }
                self.active = NO;
            }
            
            // Update intensity
            CHHapticDynamicParameter *param = [[CHHapticDynamicParameter alloc] 
                initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl 
                value:intensity 
                relativeTime:0];
            [self.player sendParameters:@[param] atTime:0 error:&error];
            if (error != nil) {
                NSLog(@"Couldn't update haptic player: %@", error.localizedDescription);
                return NO;
            }
            
            // Start playback if not active
            if (!self.active) {
                [self.player startAtTime:0 error:&error];
                self.active = YES;
            }
            
            return YES;
        }
        return NO;
    }
}

- (id)initWithController:(GCController *)controller locality:(GCHapticsLocality)locality API_AVAILABLE(macos(11.0)) {
    @autoreleasepool {
        self = [super init];
        if (self) {
            NSError *error = nil;
            __weak __typeof(self) weakSelf = self;
            
            self.engine = [controller.haptics createEngineWithLocality:locality];
            if (self.engine == nil) {
                NSLog(@"Couldn't create haptics engine for locality: %@", locality);
                return nil;
            }
            
            [self.engine startAndReturnError:&error];
            if (error != nil) {
                NSLog(@"Couldn't start haptics engine: %@", error.localizedDescription);
                return nil;
            }
            
            // Set up handlers for engine stopping/resetting
            self.engine.stoppedHandler = ^(CHHapticEngineStoppedReason stoppedReason) {
                MTYRumbleMotor *strongSelf = weakSelf;
                if (strongSelf) {
                    strongSelf.player = nil;
                    strongSelf.engine = nil;
                }
            };
            
            self.engine.resetHandler = ^{
                MTYRumbleMotor *strongSelf = weakSelf;
                if (strongSelf) {
                    strongSelf.player = nil;
                    [strongSelf.engine startAndReturnError:nil];
                }
            };
        }
        return self;
    }
}

@end

// Rumble context for a controller with low and high frequency motors
@interface MTYRumbleContext : NSObject
@property(nonatomic, strong) MTYRumbleMotor *lowFrequencyMotor;
@property(nonatomic, strong) MTYRumbleMotor *highFrequencyMotor;
@property(nonatomic, strong) GCController *controller;
@end

@implementation MTYRumbleContext

- (id)initWithController:(GCController *)controller {
    self = [super init];
    if (self) {
        self.controller = controller;
        
        if (@available(macOS 11.0, *)) {
            // Initialize rumble motors
            self.lowFrequencyMotor = [[MTYRumbleMotor alloc] 
                initWithController:controller 
                locality:GCHapticsLocalityLeftHandle];
            self.highFrequencyMotor = [[MTYRumbleMotor alloc] 
                initWithController:controller 
                locality:GCHapticsLocalityRightHandle];
            
            if (!self.lowFrequencyMotor || !self.highFrequencyMotor) {
                NSLog(@"Failed to initialize rumble motors");
                return nil;
            }
        }
    }
    return self;
}

- (BOOL)rumbleWithLowFrequency:(uint16_t)lowFrequency highFrequency:(uint16_t)highFrequency {
    if (@available(macOS 11.0, *)) {
        BOOL result = YES;
        result &= [self.lowFrequencyMotor setIntensity:(float)lowFrequency / 65535.0f];
        result &= [self.highFrequencyMotor setIntensity:(float)highFrequency / 65535.0f];
        return result;
    }
    return NO;
}

- (void)cleanup {
    if (@available(macOS 11.0, *)) {
        [self.lowFrequencyMotor cleanup];
        [self.highFrequencyMotor cleanup];
    }
}

@end

// Global storage for controller rumble contexts
static NSMutableDictionary<NSValue *, MTYRumbleContext *> *g_rumbleContexts = nil;
static dispatch_once_t g_rumbleContextsOnce;

// Initialize the global rumble contexts dictionary
static void mty_hid_gc_init(void) {
    dispatch_once(&g_rumbleContextsOnce, ^{
        g_rumbleContexts = [[NSMutableDictionary alloc] init];
    });
}

// Find GCController for a given IOHIDDevice
static GCController *mty_hid_gc_find_controller(IOHIDDeviceRef device) {
    @autoreleasepool {
        // Get vendor and product IDs from the IOHIDDevice
        NSNumber *vendorID = (__bridge NSNumber *)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey));
        NSNumber *productID = (__bridge NSNumber *)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey));
        
        if (!vendorID || !productID) {
            NSLog(@"mty_hid_gc: No vendor/product ID");
            return nil;
        }
        
        uint16_t vid = [vendorID unsignedShortValue];
        uint16_t pid = [productID unsignedShortValue];
        
        NSLog(@"mty_hid_gc: Looking for controller VID: 0x%04X, PID: 0x%04X", vid, pid);
        
        // Check if this is an Xbox controller
        if (vid != 0x045E) { // Microsoft vendor ID
            NSLog(@"mty_hid_gc: Not a Microsoft controller");
            return nil;
        }
        
        // Check for Xbox 360 wired controller PIDs - add more PIDs
        BOOL isXbox360 = (pid == 0x028E || pid == 0x028F || pid == 0x02A1 || 
                          pid == 0x0291 || pid == 0x0719 || pid == 0x02A0 ||
                          pid == 0x02DD || pid == 0x02E3 || pid == 0x02FF || pid == 0x02EA);
        if (!isXbox360) {
            NSLog(@"mty_hid_gc: Not an Xbox 360/One controller PID");
            return nil;
        }
        
        NSLog(@"mty_hid_gc: Found %lu GCControllers", (unsigned long)[[GCController controllers] count]);
        
        // Find matching GCController
        for (GCController *controller in [GCController controllers]) {
            // For Xbox controllers, GCController doesn't expose vendor/product IDs directly
            // We need to match based on the controller type
            if (@available(macOS 10.15, *)) {
                NSString *productCategory = controller.productCategory;
                NSString *vendorName = controller.vendorName;
                NSLog(@"mty_hid_gc: Checking controller - Category: %@, Vendor: %@", productCategory, vendorName);
                
                // Check for Xbox controller - be more lenient with matching
                if ([productCategory isEqualToString:@"Xbox One"] || 
                    [vendorName containsString:@"Xbox"] ||
                    [vendorName containsString:@"Microsoft"] ||
                    [productCategory containsString:@"Xbox"]) {
                    NSLog(@"mty_hid_gc: Found matching Xbox controller!");
                    // Found a potential match
                    // Store the rumble context for this controller
                    NSValue *deviceKey = [NSValue valueWithPointer:device];
                    MTYRumbleContext *context = g_rumbleContexts[deviceKey];
                    if (!context) {
                        context = [[MTYRumbleContext alloc] initWithController:controller];
                        if (context) {
                            g_rumbleContexts[deviceKey] = context;
                            NSLog(@"mty_hid_gc: Created rumble context successfully");
                        } else {
                            NSLog(@"mty_hid_gc: Failed to create rumble context");
                        }
                    }
                    return controller;
                }
            }
        }
        
        NSLog(@"mty_hid_gc: No matching GCController found");
        return nil;
    }
}

// C interface for rumble functionality
bool mty_hid_gc_rumble(void *device, uint16_t low, uint16_t high) {
    @autoreleasepool {
        // Only log non-zero rumble for less noise
        if (low > 0 || high > 0) {
            NSLog(@"mty_hid_gc_rumble: RUMBLE ON - low=%u, high=%u", low, high);
        }
        mty_hid_gc_init();
        
        IOHIDDeviceRef hidDevice = (IOHIDDeviceRef)device;
        NSValue *deviceKey = [NSValue valueWithPointer:hidDevice];
        
        // Try to get existing rumble context
        MTYRumbleContext *context = g_rumbleContexts[deviceKey];
        
        // If no context exists, try to find and create one
        if (!context) {
            NSLog(@"mty_hid_gc_rumble: No existing context, trying to find controller");
            GCController *controller = mty_hid_gc_find_controller(hidDevice);
            if (controller) {
                if (@available(macOS 11.0, *)) {
                    if (controller.haptics) {
                        context = [[MTYRumbleContext alloc] initWithController:controller];
                        if (context) {
                            g_rumbleContexts[deviceKey] = context;
                            NSLog(@"mty_hid_gc_rumble: Created new context");
                        }
                    } else {
                        NSLog(@"mty_hid_gc_rumble: Controller has no haptics support");
                    }
                }
            }
        }
        
        // Apply rumble if we have a context
        if (context) {
            NSLog(@"mty_hid_gc_rumble: Applying rumble");
            return [context rumbleWithLowFrequency:low highFrequency:high];
        }
        
        NSLog(@"mty_hid_gc_rumble: Failed - no context available");
        return false;
    }
}

// Clean up rumble context for a device
void mty_hid_gc_cleanup(void *device) {
    @autoreleasepool {
        if (g_rumbleContexts) {
            IOHIDDeviceRef hidDevice = (IOHIDDeviceRef)device;
            NSValue *deviceKey = [NSValue valueWithPointer:hidDevice];
            
            MTYRumbleContext *context = g_rumbleContexts[deviceKey];
            if (context) {
                [context cleanup];
                [g_rumbleContexts removeObjectForKey:deviceKey];
            }
        }
    }
}

// Check if GCController rumble is available for a device
bool mty_hid_gc_rumble_available(void *device) {
    @autoreleasepool {
        if (@available(macOS 11.0, *)) {
            IOHIDDeviceRef hidDevice = (IOHIDDeviceRef)device;
            GCController *controller = mty_hid_gc_find_controller(hidDevice);
            return (controller && controller.haptics != nil);
        }
        return false;
    }
}