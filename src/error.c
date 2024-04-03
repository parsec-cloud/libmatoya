#include "error.h"

#include "tlocal.h"

static TLOCAL MTY_Queue *Q = NULL;

void error_local_init(void)
{
	error_local_clear();

	Q = MTY_QueueCreate(10, sizeof(MTY_Error));
}

void error_local_clear(void)
{
	MTY_QueueDestroy(&Q);
}


bool error_local_get_next_error(MTY_Error *error)
{
	if (!Q)
		return false;

	MTY_Error *buf = NULL;

	bool rc = MTY_QueueGetOutputBuffer(Q, 0, (void **) &buf, NULL);
	if (rc) {
		*error = *buf;
		MTY_QueuePop(Q);
	}

	return rc;
}

void error_local_push_error(MTY_Error error)
{
	if (!Q)
		return;

	MTY_Error *buf = MTY_QueueGetInputBuffer(Q);
	if (buf) {
		*buf = error;
		MTY_QueuePush(Q, sizeof(MTY_Error));
	}
}
