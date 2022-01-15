#ifndef COMMAND_H
#define COMMAND_H

#include <sds.h>

#include "connect.h"

void process_cmd(sds command, connect_info_handle cinfo);

#endif