/** \file pnp_ids.h
 *
 *  Provides a lookup table of 3 character manufacturer codes,
 *  which are used, e.g. in EDIDs.
 */

// Copyright (C) 2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PNP_IDS_H_
#define PNP_IDS_H_

char * pnp_name(char * id);

#ifdef TESTS
void pnp_id_tests();
#endif

#endif /* PNP_IDS_H_ */
