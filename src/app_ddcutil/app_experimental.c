/** \file app_experimental.c */

// Copyright (C) 2021-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdio.h>
#include <strings.h>

#include "util/report_util.h"
#include "util/string_util.h"
#include "util/timestamp.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_strategy_dispatcher.h"

#include "ddc/ddc_displays.h"

#include "app_experimental.h"


#define REPORT_FLAG_OPTION(_flagno, _action) \
rpt_vstring(depth+1, "Utility option --f"#_flagno" %s %s",   \
     (parsed_cmd->flags & CMD_FLAG_F##_flagno ) ? "enabled: " : "disabled:", _action)

void
report_experimental_options(Parsed_Cmd * parsed_cmd, int depth)
{
   rpt_label(depth, "Experimental Options:");
   REPORT_FLAG_OPTION(1, "EDID read uses I2C layer");
   REPORT_FLAG_OPTION(2, "Experimental sysfs analysis");    // was Filter phantom displays
   REPORT_FLAG_OPTION(3, "Unused");
   REPORT_FLAG_OPTION(4, "Read strategy tests");
   REPORT_FLAG_OPTION(5, "Unused");
   REPORT_FLAG_OPTION(6, "Unused");

   rpt_vstring(depth+1, "Utility option --i1 = %d:     Unused", parsed_cmd->i1);
   rpt_nl();
}

#undef REPORT_FLAG_OPTION


bool init_experimental_options(Parsed_Cmd* parsed_cmd)
{
   if (parsed_cmd->flags & CMD_FLAG_F1) {
      fprintf(stdout, "EDID reads will use normal I2C calls\n");
      EDID_Read_Uses_I2C_Layer = true;
   }

   // if (parsed_cmd->flags & CMD_FLAG_F2) {
   //    fprintf(stdout, "Filter phantom displays\n");
   //   check_phantom_displays = true;    // extern in ddc_displays.h
   // }

   if (parsed_cmd->flags & CMD_FLAG_F3) {
      fprintf(stdout, "Write trace messages to syslog\n");
   }

   return true;
}


//
// Test display detection variants
//

typedef enum {
   _DYNAMIC = 0,
   _128     = 128,
   _256     = 256
} Edid_Read_Size_Option;

static char * read_size_name(int n) {
   char * result = "WTF";
   switch (n) {
   case   0: result = "dynamic";  break;
   case 128: result = "128";      break;
   case 256: result = "256";      break;
   default:  result = "INVALID";  break;
   }
   return result;
}


/** Tests for display detection variants.
 *
 *  Controlled by utility option --f4
 */
void test_display_detection_variants() {

   typedef enum {
      _FALSE,
      _TRUE,
      _DNA
   } Bytewise_Option;

   typedef struct {
      I2C_IO_Strategy_Id     i2c_io_strategy_id;
      bool                   edid_uses_i2c_layer;
      Bytewise_Option        edid_read_bytewise;    // applies when edid_uses_i2c_layer == FALSE
      Bytewise_Option        i2c_read_bytewise;     // applies when edid_uses_i2c_layer == TRUE
      bool                   write_before_read;
      Edid_Read_Size_Option  edid_read_size;
   } Choice_Entry;

   typedef struct {
      int      valid_display_ct;
      uint64_t elapsed_nanos;
   } Choice_Results;

   char * choice_name[] = {"false", "true", "DNA"};

   // char * read_size_name[] = {"dynamic", "128", "256"};

   Choice_Entry choices[] =
   //                          use I2c edid        i2c          write     EDID Read
   // i2c_io_strategy          layer   bytewise    bytewise     b4 read   Size
   // ================         ======  ========     =======     =======   ========
   {
#ifndef I2C_IO_IOCTL_ONLY
     {I2C_IO_STRATEGY_FILEIO,  false,   _FALSE,      _DNA,      _FALSE,   _128},
     {I2C_IO_STRATEGY_FILEIO,  false,   _FALSE,      _DNA,      _FALSE,   _256},
     {I2C_IO_STRATEGY_FILEIO,  false,   _FALSE,      _DNA,      _FALSE,   _DYNAMIC},

     {I2C_IO_STRATEGY_FILEIO,  false,   _FALSE,      _DNA,      _TRUE,    _128},
     {I2C_IO_STRATEGY_FILEIO,  false,   _FALSE,      _DNA,      _TRUE,    _256},
     {I2C_IO_STRATEGY_FILEIO,  false,   _FALSE,      _DNA,      _TRUE,    _DYNAMIC},

     {I2C_IO_STRATEGY_FILEIO,  false,   _TRUE,       _DNA,      _FALSE,   _128},
     {I2C_IO_STRATEGY_FILEIO,  false,   _TRUE,       _DNA,      _FALSE,   _256},
     {I2C_IO_STRATEGY_FILEIO,  false,   _TRUE,       _DNA,      _FALSE,   _DYNAMIC},

     {I2C_IO_STRATEGY_FILEIO,  false,   _TRUE,       _DNA,      _TRUE,    _128},
     {I2C_IO_STRATEGY_FILEIO,  false,   _TRUE,       _DNA,      _TRUE,    _256},
     {I2C_IO_STRATEGY_FILEIO,  false,   _TRUE,       _DNA,      _TRUE,    _DYNAMIC},

     {I2C_IO_STRATEGY_FILEIO,  true,    _DNA,        _DNA,      _FALSE,   _128},
     {I2C_IO_STRATEGY_FILEIO,  true,    _DNA,        _DNA,      _FALSE,   _256},
     {I2C_IO_STRATEGY_FILEIO,  true,    _DNA,        _DNA,      _FALSE,   _DYNAMIC},

     {I2C_IO_STRATEGY_FILEIO,  true,    _DNA,        _DNA,      _TRUE,    _128},
     {I2C_IO_STRATEGY_FILEIO,  true,    _DNA,        _DNA,      _TRUE,    _256},
     {I2C_IO_STRATEGY_FILEIO,  true,    _DNA,        _DNA,      _TRUE,    _DYNAMIC},

     {I2C_IO_STRATEGY_IOCTL,   false,   _FALSE,      _DNA,      _FALSE,   _128},
     {I2C_IO_STRATEGY_IOCTL,   false,   _FALSE,      _DNA,      _FALSE,   _256},
     {I2C_IO_STRATEGY_IOCTL,   false,   _FALSE,      _DNA,      _FALSE,   _DYNAMIC},

     {I2C_IO_STRATEGY_IOCTL,   false,   _FALSE,      _DNA,      _TRUE,    _128},
     {I2C_IO_STRATEGY_IOCTL,   false,   _FALSE,      _DNA,      _TRUE,    _256},
     {I2C_IO_STRATEGY_IOCTL,   false,   _FALSE,      _DNA,      _TRUE,    _DYNAMIC},

     {I2C_IO_STRATEGY_IOCTL,   false,   _TRUE,       _DNA,      _FALSE,   _128},
     {I2C_IO_STRATEGY_IOCTL,   false,   _TRUE,       _DNA,      _FALSE,   _256},
     {I2C_IO_STRATEGY_IOCTL,   false,   _TRUE,       _DNA,      _FALSE,   _DYNAMIC},

     {I2C_IO_STRATEGY_IOCTL,   false,   _TRUE,       _DNA,      _TRUE,    _128},
     {I2C_IO_STRATEGY_IOCTL,   false,   _TRUE,       _DNA,      _TRUE,    _256},
     {I2C_IO_STRATEGY_IOCTL,   false,   _TRUE,       _DNA,      _TRUE,    _DYNAMIC},
#endif
     {I2C_IO_STRATEGY_IOCTL,   true,    _DNA,        _DNA,      _FALSE,   _128},
     {I2C_IO_STRATEGY_IOCTL,   true,    _DNA,        _DNA,      _FALSE,   _256},
     {I2C_IO_STRATEGY_IOCTL,   true,    _DNA,        _DNA,      _FALSE,   _DYNAMIC},

     {I2C_IO_STRATEGY_IOCTL,   true,    _DNA,        _DNA,      _TRUE,    _128},
     {I2C_IO_STRATEGY_IOCTL,   true,    _DNA,        _DNA,      _TRUE,    _256},
     {I2C_IO_STRATEGY_IOCTL,   true,    _DNA,        _DNA,      _TRUE,    _DYNAMIC},
   };
   int choice_ct = ARRAY_SIZE(choices);

   Choice_Results results[ARRAY_SIZE(choices)];

   int d = 1;
   for (int ndx=0; ndx<choice_ct; ndx++) {
      // sleep_millis(1000);
      Choice_Entry   cur        = choices[ndx];
      Choice_Results* cur_result = &results[ndx];

      rpt_nl();
      rpt_vstring(0, "===========> IO STRATEGY %d:", ndx+1);
       char * s = (cur.i2c_io_strategy_id == I2C_IO_STRATEGY_FILEIO) ? "FILEIO" : "IOCTL";
       rpt_vstring(d, "i2c_io_strategy:          %s", s);

       rpt_vstring(d, "EDID read uses I2C layer: %s", (cur.edid_uses_i2c_layer) ? "I2C Layer" : "Directly"); // SBOOL(cur.edid_uses_i2c_layer));

    // rpt_vstring(d, "i2c_read_bytewise:        %s", choice_name[cur.i2c_read_bytewise]);
       rpt_vstring(d, "EDID read bytewise:       %s", choice_name[cur.edid_read_bytewise]);
       rpt_vstring(d, "write before read:        %s", SBOOL(cur.write_before_read));
       rpt_vstring(d, "EDID read size:           %s", read_size_name(cur.edid_read_size));

       i2c_set_io_strategy(       cur.i2c_io_strategy_id);
       EDID_Read_Uses_I2C_Layer = cur.edid_uses_i2c_layer;
       I2C_Read_Bytewise        = false;       //      cur.i2c_read_bytewise;
       EDID_Read_Bytewise       = cur.edid_read_bytewise;
       EDID_Read_Size           = cur.edid_read_size;
       assert(EDID_Read_Size == 128 || EDID_Read_Size == 256 || EDID_Read_Size == 0);

       // discard existing detected monitors
       ddc_discard_detected_displays();
       uint64_t start_time = cur_realtime_nanosec();
       ddc_ensure_displays_detected();
       int valid_ct = ddc_get_display_count(/*include_invalid_displays*/ false);
       uint64_t end_time = cur_realtime_nanosec();
       cur_result->elapsed_nanos = end_time-start_time;
       rpt_vstring(d, "Valid displays:           %d", valid_ct);
       cur_result->valid_display_ct = valid_ct;
       rpt_vstring(d, "Elapsed time:           %s seconds", formatted_time(end_time - start_time));
       rpt_nl();
       // will include any USB or ADL displays, but that's ok
       ddc_report_displays(/*include_invalid_displays=*/ true, 0);
   }

   rpt_label(  d, "SUMMARY");
   rpt_nl();
   // will be wrong for our purposes if same monitor appears on 2 i2c buses
   // int total_displays = get_sysfs_drm_edid_count();

   // ddc_discard_detected_displays();
   // ddc_ensure_displays_detected();  // to perform normal detection
   // int total_displays = get_display_count(/*include_invalid_displays*/ true);
   // rpt_vstring(d, "Total Displays (per /sys/class/drm): %d", total_displays);
   rpt_nl();

   rpt_vstring(d, "   I2C IO    EDID        EDID Read   Write    EDID Read Valid    Seconds");
   rpt_vstring(d, "   Strategy  Method      Bytewise    b4 Read  Size      Displays         ");
   rpt_vstring(d, "   =======   ========    =========   =======  ========= ======== =======");
   for (int ndx = 0; ndx < choice_ct; ndx++) {
      Choice_Entry cur = choices[ndx];
      Choice_Results* cur_result = &results[ndx];

      rpt_vstring(d, "%2d %-7s   %-9s   %-7s     %-5s    %-7s %3d      %s",
            ndx+1,
            (cur.i2c_io_strategy_id == I2C_IO_STRATEGY_FILEIO) ? "FILEIO" : "IOCTL",
            (cur.edid_uses_i2c_layer) ? "I2C Layer" : "Directly",
        //    choice_name[cur.i2c_read_bytewise],
            choice_name[cur.edid_read_bytewise],
            SBOOL(cur.write_before_read),
            read_size_name(cur.edid_read_size),
            cur_result->valid_display_ct,
            formatted_time(cur_result->elapsed_nanos));
   }
   rpt_nl();
#ifdef DO_NOT_DISTRIBUTE
   rpt_label(d, "Failures");
   rpt_nl();
   for (int ndx = 0; ndx < choice_ct; ndx++) {
      Choice_Entry cur = choices[ndx];
      Choice_Results* cur_result = &results[ndx];

      if (cur_result->valid_display_ct < 3)
      rpt_vstring(d, "%2d %-7s   %-9s   %-7s     %-5s    %-7s %3d      %s",
            ndx+1,
            (cur.i2c_io_strategy_id == I2C_IO_STRATEGY_FILEIO) ? "FILEIO" : "IOCTL",
            (cur.edid_uses_i2c_layer) ? "I2C Layer" : "Directly",
        //    choice_name[cur.i2c_read_bytewise],
            choice_name[cur.edid_read_bytewise],
            SBOOL(cur.write_before_read),
            read_size_name(cur.edid_read_size),
            cur_result->valid_display_ct,
            formatted_time(cur_result->elapsed_nanos));
   }
#endif
}
