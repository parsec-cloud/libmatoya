#pragma once

#include "matoya.h"

void error_local_init(void);
void error_local_clear(void);

bool error_local_get_next_error(MTY_Error *error);
void error_local_push_error(MTY_Error error);
