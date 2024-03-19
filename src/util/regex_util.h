// regex_util.h

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef REGEX_UTIL_H_
#define REGEX_UTIL_H_

#include <regex.h>
#include <stdint.h>

void free_regex_hash_table();

bool compile_and_eval_regex(const char * pattern, const char * value);

bool compile_and_eval_regex_with_matches(
      const char * pattern,
      const char * value,
      size_t       max_matches,
      regmatch_t * pm);

#endif /* REGEX_UTIL_H_ */
