#ifndef HANDLERS_H
#define HANDLERS_H

#include "connect.h"

void handler_NICK(connect_info_handle cinfo, connect_info_handle connections, sds command);

void handler_USER(connect_info_handle cinfo, connect_info_handle connections, sds command);

#endif