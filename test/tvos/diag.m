// Polls every controller GameController knows about, independent of libmatoya's
// connect notifications and valueChangedHandler. Useful when no controller events
// arrive: it shows whether the controller is visible to the OS at all.
#include <stdio.h>
#include <GameController/GameController.h>

void spike_diag_controllers(void)
{
	GCController *current = nil;
	if (@available(tvOS 14.0, iOS 14.0, *))
		current = GCController.current;

	NSArray<GCController *> *all = [GCController controllers];
	printf("[diag] GCController.controllers: %lu\n", (unsigned long) all.count);

	for (GCController *gc in all) {
		GCExtendedGamepad *gp = gc.extendedGamepad;
		printf("[diag]   vendor='%s' category='%s' current=%d extended=%d",
			gc.vendorName.UTF8String, gc.productCategory.UTF8String, gc == current, gp != nil);

		if (gp)
			printf(" A=%d B=%d LX=%+.2f LY=%+.2f RT=%.2f handler=%d",
				gp.buttonA.pressed, gp.buttonB.pressed, gp.leftThumbstick.xAxis.value,
				gp.leftThumbstick.yAxis.value, gp.rightTrigger.value, gp.valueChangedHandler != nil);

		printf("\n");
	}

	fflush(stdout);
}
