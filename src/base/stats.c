// stats.c

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "stats.h"
 
#ifndef STATS_H
#define STATS_H


static char * retry_type_descriptions[] = {
      "write only",
      "write-read",
      "multi-part read",
      "multi-part write"
};

static char * retry_type_names[] = {
      "WRITE_ONLY_TRIES_OP",
      "WRITE_READ_TRIES_OP",
      "MULTI_PART_READ_OP",
      "MULTI_PART_WRITE_OP"
};

const char * retry_type_name(Retry_Operation type_id) {
   return retry_type_names[type_id];
}

const char * retry_type_description(Retry_Operation type_id) {
   return retry_type_descriptions[type_id];
}

#endif
