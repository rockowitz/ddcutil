   // typedef struct {
   //    a    int;
   //    b    int;
   //   } Footype_t;
    extern "Python" int sample_callback(int status_code);


    DDCA_Status
    ddca_pass_callback(
          //Simple_Callback_Func  func,
          int (*func)(int),
          int                   parm
          );
