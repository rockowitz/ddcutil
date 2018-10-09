// i2c_testutil.c

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "util/data_structures.h"
#include "util/string_util.h"

#include "i2c/wrap_i2c-dev.h"

#include "i2c/i2c_bus_core.h"

#include "i2c_testutil.h"



// Functions and data structures for interpreting the I2C bus functionality flags.
// They are overly complex for production use.  They were created during development
// to facilitate exploratory programming.

// Note 2 entries for I2C_FUNC_I2C.  Usage must take this into account.
Value_Name_Title_Table functionality_table2 = {
      //  flag                              I2C function name
      VNT(I2C_FUNC_I2C                    , "ioctl_write"),
      VNT(I2C_FUNC_I2C                    , "ioctl_read"),
      VNT(I2C_FUNC_10BIT_ADDR             , NULL),
      VNT(I2C_FUNC_PROTOCOL_MANGLING      , NULL),
      VNT(I2C_FUNC_SMBUS_PEC              , "i2c_smbus_pec"),
      VNT(I2C_FUNC_SMBUS_BLOCK_PROC_CALL  , "i2c_smbus_block_proc_call"),
      VNT(I2C_FUNC_SMBUS_QUICK            , "i2c_smbus_quick"),
      VNT(I2C_FUNC_SMBUS_READ_BYTE        , "i2c_smbus_read_byte"),
      VNT(I2C_FUNC_SMBUS_WRITE_BYTE       , "i2c_smbus_write_byte"),
      VNT(I2C_FUNC_SMBUS_READ_BYTE_DATA   , "i2c_smbus_read_byte_data"),
      VNT(I2C_FUNC_SMBUS_WRITE_BYTE_DATA  , "i2c_smbus_write_byte_data"),
      VNT(I2C_FUNC_SMBUS_READ_WORD_DATA   , "i2c_smbus_read_word_data"),
      VNT(I2C_FUNC_SMBUS_WRITE_WORD_DATA  , "i2c_smbus_write_word_data"),
      VNT(I2C_FUNC_SMBUS_PROC_CALL        , "i2c_smbus_proc_call"),
      VNT(I2C_FUNC_SMBUS_READ_BLOCK_DATA  , "i2c_smbus_read_block_data"),
      VNT(I2C_FUNC_SMBUS_WRITE_BLOCK_DATA , "i2c_smbus_write_block_data"),
      VNT(I2C_FUNC_SMBUS_READ_I2C_BLOCK   , "i2c_smbus_read_i2c_block_data"),
      VNT(I2C_FUNC_SMBUS_WRITE_I2C_BLOCK  , "i2c_smbus_write_i2c_block_data"),
      VNT_END
};


//
// For test driver use only
//

static bool is_function_supported(int busno, char * funcname) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d, funcname=%s", busno, funcname);

   bool result = true;
   if ( !streq(funcname, "read") &&  !streq(funcname, "write") ) {
      uint32_t func_bit = vnt_find_id(
                               functionality_table2,
                               funcname,
                               true,      //  search title field
                               false,     //  ignore_case,
                               0x00);     //   default_id);

      if (!func_bit) {
         DBGMSG("Unrecognized function name: %s", funcname);
         result = false;
         goto bye;
      }

      // DBGMSG("functionality=0x%lx, func_table_entry->bit=-0x%lx", bus_infos[busno].functionality, func_table_entry->bit);
      // Bus_Info * bus_info = i2c_get_bus_info(busno, DISPSEL_NONE);
      // Bus_Info * bus_info = i2c_get_bus_info_new(busno);
      I2C_Bus_Info * bus_info = detect_single_bus(busno);
      if ( !bus_info ) {
         DBGMSG("Invalid bus: /dev/i2c-%d", busno);
         result = false;
      }
      else   // add unneeded else clause to avoid clang warning
         result = (bus_info->functionality & func_bit) != 0;
      i2c_free_bus_info(bus_info);
   }

bye:
   DBGMSF(debug, "busno=%d, funcname=%s, returning %d", busno, funcname, result);
   return result;
}


/** Verify that the specified I2C write and read functions are supported.
 *
 *  This function is used in test management.
 *
 *  @param  busno   I2C bus number
 *  @param  write_func_name  write function name
 *  @param  read_func_name   read function name
 *
 *  @return true/false
 */
bool i2c_verify_functions_supported(int busno, char * write_func_name, char * read_func_name) {
   // printf("(%s) Starting. busno=%d, write_func_name=%s, read_func_name=%s\n",
   //        __func__, busno, write_func_name, read_func_name);
   bool write_supported = is_function_supported(busno, write_func_name);
   bool read_supported  = is_function_supported(busno, read_func_name);

   if (!write_supported)
      printf("Unsupported write function: %s\n", write_func_name );
   if (!read_supported)
      printf("Unsupported read function: %s\n", read_func_name );

   bool result =write_supported && read_supported;
   // DBGMSG("returning %d", result);
   return result;
}
