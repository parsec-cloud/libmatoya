#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

int main(int argc, char **argv) {
    @autoreleasepool {
        // Monitor for controller connections
        [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification 
            object:nil 
            queue:nil 
            usingBlock:^(NSNotification *note) {
                GCController *controller = note.object;
                NSLog(@"Controller connected: %@ - %@", controller.vendorName, controller.productCategory);
            }];
        
        // List current controllers
        NSArray *controllers = [GCController controllers];
        NSLog(@"Currently connected controllers: %lu", (unsigned long)controllers.count);
        
        for (GCController *controller in controllers) {
            NSLog(@"Controller: %@ - Category: %@", controller.vendorName, controller.productCategory);
            
            if (@available(macOS 11.0, *)) {
                if (controller.haptics) {
                    NSLog(@"  Has haptics support!");
                    
                    // Try to create and test rumble
                    NSError *error = nil;
                    CHHapticEngine *engine = [controller.haptics createEngineWithLocality:GCHapticsLocalityDefault];
                    if (engine) {
                        [engine startAndReturnError:&error];
                        if (!error) {
                            NSLog(@"  Successfully started haptic engine!");
                            
                            // Create a simple rumble pattern
                            CHHapticEventParameter *intensity = [[CHHapticEventParameter alloc] 
                                initWithParameterID:CHHapticEventParameterIDHapticIntensity value:0.5];
                            CHHapticEvent *event = [[CHHapticEvent alloc] 
                                initWithEventType:CHHapticEventTypeHapticContinuous 
                                parameters:@[intensity] 
                                relativeTime:0 
                                duration:0.5];
                            CHHapticPattern *pattern = [[CHHapticPattern alloc] 
                                initWithEvents:@[event] 
                                parameters:@[] 
                                error:&error];
                            
                            if (!error) {
                                id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:&error];
                                if (!error) {
                                    NSLog(@"  Testing rumble for 0.5 seconds...");
                                    [player startAtTime:0 error:&error];
                                    [NSThread sleepForTimeInterval:1.0];
                                    [player stopAtTime:0 error:&error];
                                    NSLog(@"  Rumble test complete!");
                                }
                            }
                        }
                    }
                } else {
                    NSLog(@"  No haptics support");
                }
            }
        }
        
        // Keep running for a bit to detect controllers
        NSLog(@"Waiting 5 seconds for controller detection...");
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:5.0]];
        
        // Check again
        controllers = [GCController controllers];
        NSLog(@"Final controller count: %lu", (unsigned long)controllers.count);
        
        // Test rumble on all controllers
        for (GCController *controller in controllers) {
            NSLog(@"Testing rumble on: %@ - %@", controller.vendorName, controller.productCategory);
            
            if (@available(macOS 11.0, *)) {
                if (controller.haptics) {
                    NSError *error = nil;
                    CHHapticEngine *engine = [controller.haptics createEngineWithLocality:GCHapticsLocalityDefault];
                    if (engine) {
                        [engine startAndReturnError:&error];
                        if (!error) {
                            // Create a simple rumble pattern
                            CHHapticEventParameter *intensity = [[CHHapticEventParameter alloc] 
                                initWithParameterID:CHHapticEventParameterIDHapticIntensity value:1.0];
                            CHHapticEvent *event = [[CHHapticEvent alloc] 
                                initWithEventType:CHHapticEventTypeHapticContinuous 
                                parameters:@[intensity] 
                                relativeTime:0 
                                duration:1.0];
                            CHHapticPattern *pattern = [[CHHapticPattern alloc] 
                                initWithEvents:@[event] 
                                parameters:@[] 
                                error:&error];
                            
                            if (!error) {
                                id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:&error];
                                if (!error) {
                                    NSLog(@"  RUMBLING NOW for 1 second...");
                                    [player startAtTime:0 error:&error];
                                    [NSThread sleepForTimeInterval:1.5];
                                    NSLog(@"  Rumble complete!");
                                }
                            }
                        }
                    }
                } else {
                    NSLog(@"  No haptics support");
                }
            } else {
                NSLog(@"  macOS 11.0+ required for haptics");
            }
        }
    }
    
    return 0;
}