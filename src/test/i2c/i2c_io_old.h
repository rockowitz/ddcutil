#ifndef PROBEI2C_I2C_IO
#define PROBEI2C_I2C_IO

#include <stdbool.h>

#include <base/ddc_base_defs.h>
#include <base/execution_stats.h>
#include <base/common.h>
#include <base/msg_control.h>
#include <base/status_code_mgt.h>
#include <base/util.h>

#include <i2c/i2c_base_io.h>

// was in common.h
#define MAX_I2C_MESSAGE_SIZE   131    // 127 + 4;

void set_i2c_write_mode(char* mode);

void set_i2c_read_mode(char* mode);


Global_Status_Code call_i2c_writer(
      I2C_Writer writer,
      char * writer_name,
      int    fh,
      int    bytect,
      Byte * bytes_to_write,
      int    sleep_millisec)
;



Global_Status_Code call_i2c_reader(
       I2C_Reader reader,
       char *     reader_name,
       int        fh,
       int        bytect,
       Byte *     readbuf,
       int        sleep_millisec);


//
// Write to I2C bus
//

Global_Status_Code do_i2c_file_write(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec);

Global_Status_Code do_i2c_smbus_write_i2c_block_data(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec);

Global_Status_Code do_i2c_ioctl_write(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec);

Global_Status_Code perform_i2c_write(int fh, char * write_mode, int bytect, Byte * bytes_to_write, int sleep_millisec);

Global_Status_Code perform_i2c_write2(int fh, int bytect, Byte * bytes_to_write, int sleep_millisec);

//
// Read from I2C bus
//

Global_Status_Code do_i2c_file_read(int fh, int bytect, Byte * readbuf, int sleep_millisec);

Global_Status_Code do_i2c_smbus_read_i2c_block_data(int fh, int bytect, Byte * readbuf, int sleep_millisec);

Global_Status_Code do_i2c_ioctl_read(int fh, int bytect, Byte * readbuf, int sleep_millisec);

Global_Status_Code perform_i2c_read(
                 int    fh,
                 char * read_mode,
                 int    bytect,
                 Byte * readbuf,
                 int    sleep_millisec
                );

Global_Status_Code perform_i2c_read2(int fh, int bytect, Byte * readbuf, int sleep_millisec);



#endif
