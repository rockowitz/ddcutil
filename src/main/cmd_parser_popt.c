/*
 * cmd_parser_popt.c
 *
 *  Created on: Jun 16, 2014
 *      Author: rock
 */

#include <assert.h>
#include <ctype.h>
#include <popt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <base/common.h>
#include <base/displays.h>
#include <base/msg_control.h>
#include <util/report_util.h>
#include <base/util.h>

#include <main/cmd_parser_popt.h>

//
// Command Description data structure
//

typedef
struct {
   int          cmd_id;
   const char * cmd_name;
   int          minchars;
   int          min_arg_ct;
   int          max_arg_ct;
} Cmd_Desc;


static Cmd_Desc cmdinfo[] = {
 // cmd_id              cmd_name  minchars min_arg_ct max_arg_ct
   {CMDID_DETECT,       "detect",        3,  0,       0},
// {CMDID_INFO,         "information",   3,  0,       0},
   {CMDID_CAPABILITIES, "capabilities",  3,  0,       0},
   {CMDID_GETVCP,       "getvcp",        3,  1,       1},
   {CMDID_SETVCP,       "setvcp",        3,  2,       MAX_SETVCP_VALUES*2},
   {CMDID_LISTVCP,      "listvcp",       5,  0,       0},
   {CMDID_TESTCASE,     "testcase",      3,  1,       1},
   {CMDID_LISTTESTS,    "listtests",     5,  0,       0},
   {CMDID_LOADVCP,      "loadvcp",       3,  1,       1},
   {CMDID_DUMPVCP,      "dumpvcp",       3,  0,       1},
};
static int cmdct = sizeof(cmdinfo)/sizeof(Cmd_Desc);


void validate_cmdinfo() {
   int ndx = 0;
   for (; ndx < cmdct; ndx++) {
      assert( cmdinfo[ndx].max_arg_ct <= MAX_ARGS);
   }
}


void show_cmd_desc(Cmd_Desc * cmd_desc) {
   printf("CmdDesc at %p\n", cmd_desc);
   printf("   cmd_id:     %d\n", cmd_desc->cmd_id);
   printf("   cmd_name:   %s\n", cmd_desc->cmd_name);
   printf("   minchars:   %d\n", cmd_desc->minchars);
   printf("   min_arg_ct: %d\n", cmd_desc->min_arg_ct);
   printf("   max_arg_ct: %d\n", cmd_desc->max_arg_ct);
}


Cmd_Desc * find_command(char * cmd) {
   Cmd_Desc * result = NULL;
   int ndx = 0;
   for (; ndx < cmdct; ndx++) {
      Cmd_Desc desc = cmdinfo[ndx];
      if (is_abbrev(cmd, desc.cmd_name, desc.minchars)) {
         result = &cmdinfo[ndx];
      }
   }
   // printf("(%s) cmd=|%s|, returning %p\n", __func__, cmd, result);
   return result;
}


Cmd_Desc * get_command(int cmdid) {
   bool debug = true;
   Cmd_Desc * result = NULL;
   int ndx = 0;
   for (; ndx < cmdct; ndx++) {
      Cmd_Desc desc = cmdinfo[ndx];
      if (cmdid == desc.cmd_id)  {
         result = &cmdinfo[ndx];
      }
   }
   if (debug) {
      printf("(%s) cmdid=|%d|, returning %p\n", __func__, cmdid, result);
      show_cmd_desc(result);
   }
   return result;
}



//
// Parsed_Cmd data structure
//

#ifdef REFERENCE
typedef
struct {
   char               marker[4];      // always PCMD
   int                cmd_id;
   int                argct;
   char *             args[MAX_ARGS];
   bool               stats;
   bool               ddcdata;
   MsgLevel           msg_level;
   bool               programmatic_output;
   bool               force;
   DisplayIdentifier* pdid;
   Trace_Group        trace;
   Output_Lvel        output_level;
} Parsed_Cmd;
#endif


/* Allocates new Parsed_Cmd data structure, sets default values.
 *
 * Returns:
 *    initialized ParsedCmd
 */
Parsed_Cmd *  new_parsed_cmd() {
   Parsed_Cmd * parsed_cmd = calloc(1, sizeof(Parsed_Cmd));
   memcpy(parsed_cmd->marker, "PCMD", 4);
#ifdef OLD
   parsed_cmd->msg_level = NORMAL;
#endif
   // n. all flags are false, byte values 0, integers 0, pointers NULL because of calloc
   parsed_cmd->output_level = OL_DEFAULT;
   return parsed_cmd;
}


// Debugging function
void show_parsed_cmd(Parsed_Cmd * parsed_cmd) {
   int depth=0;
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("Parsed_Cmd", parsed_cmd, depth);
   rpt_int( "cmd_id",      NULL,  parsed_cmd->cmd_id,                 d1);
   // rptStructureLoc("dref", parsed_cmd->dref,                       d1);
   // if (parsed_cmd->dref)
   //    report_display_ref(parsed_cmd->dref, d2);
   rpt_structure_loc("pdid", parsed_cmd->pdid,                        d1);
   if (parsed_cmd->pdid)
       report_display_identifier(parsed_cmd->pdid,                    d2);
   rpt_bool("stats",       NULL,  parsed_cmd->stats,                  d1);
   rpt_bool("ddcdata",     NULL,  parsed_cmd->ddcdata,                d1);
#ifdef OLD
   rpt_str( "msg_level",   NULL,  msg_level_name(parsed_cmd->msg_level), d1);
   rpt_bool("prog output", NULL,  parsed_cmd->programmatic_output,    d1);
#endif
   rpt_str( "output_level",NULL,  output_level_name(parsed_cmd->output_level), d1);
   rpt_bool("force",       NULL,  parsed_cmd->force,                  d1);
   rpt_int_as_hex(
            "trace",       NULL,  parsed_cmd->trace,                  d1);
   rpt_int( "argct",       NULL,  parsed_cmd->argct,                  d1);
   int ndx = 0;
   for (ndx = 0; ndx < parsed_cmd->argct; ndx++) {
      printf("  argument %d:  %s\n", ndx, parsed_cmd->args[ndx]);
   }
}


bool all_digits(char * val, int ct) {
   bool debug = false;
   if (debug)
      printf("(%s) ct-%d, val -> |%.*s|\n", __func__, ct, ct, val );
   bool ok = true;
   int ndx;
   for (ndx = 0; ndx < ct; ndx++) {
      if ( !isdigit(*(val+ndx)) ) {
         ok = false;
         break;
      }
   }
   if (debug)
      printf("(%s) Returning: %d  \n", __func__, ok );
   return ok;
}


bool parse_adl_arg(char * val, int * piAdapterIndex, int * piDisplayIndex) {
   int rc = sscanf(val, "%d.%d", piAdapterIndex, piDisplayIndex);
   // printf("(%s) val=|%s| sscanf() returned %d  \n", __func__, val, rc );
   bool ok = (rc == 2);
   return ok;
}


bool parse_int_arg(char * val, int * pIval) {
   int ct = sscanf(val, "%d", pIval);
   return (ct == 1);
}


/* Primary parsing function
 *
 * Arguments:
 *    argc      number of command line arguments
 *    argv      array of pointers to command line arguments
 *
 * Returns:
 *    pointer to a ParsedCmd data structure if parsing successful
 *    NULL if execution should be terminated
 */
Parsed_Cmd * parse_command(int argc, char * argv[]) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting\n", __func__ );
   validate_cmdinfo();   // assertions

   Parsed_Cmd * parsed_cmd = new_parsed_cmd();
   parsed_cmd->pdid = create_busno_display_identifier(0);    // default monitor

   char * adlwork  = "default adlwork";
   char * edidwork = "default edidwork";
   int    buswork = 0;
   char * modelwork = NULL;
   char * snwork    = NULL;
   char * tracework = "";

   // Define options for popt

   /*  for reference:
   struct poptOption {
       const char * longName;      // long option name, NULL for no long name
       char         shortName;     // short 1 character name, '\0' for no short name
       int          argInfo;
       void *       arg;
       int          val;           // option identifier, used in callbacks
       const char * descrip;       // describes the option in autohelp
       const char * argDescrip;    // identifier for argument value, if applicable
   };
   */

   // n. val is an integer value
   struct poptOption po[] =  {
       //             arginfo              &arg                 val  --help description             argument-description
         {"bus",     'b', POPT_ARG_INT,    &buswork,            'B', "I2C bus number",              "busNum" },
         {"adl",     'a', POPT_ARG_STRING, &adlwork,            'A', "ADL adapter and display indexes",  "adapterNum.displayNum"},
//       {"stats",   's', POPT_ARG_NONE,   &parsedCmd->stats,   'S', "Show retry statistics",       NULL},
         {"stats",   's', POPT_ARG_NONE,   NULL,                'S', "Show retry statistics",       NULL},
         {"ddc",     'c', POPT_ARG_NONE,   NULL               , 'C', "Show recoverable DDC errors", NULL},
         {"ddcdata", '\0',POPT_ARG_NONE,   NULL               , 'C', "Show recoverable DDC errors", NULL},
 //      {"ddc",     'c', POPT_ARG_NONE,   &parsedCmd->ddcdata, 'C', "Show recoverable DDC errors", NULL},
 //      {"ddcdata", 'c', POPT_ARG_NONE,   &parsedCmd->ddcdata, 'C', "Show recoverable DDC errors", NULL},
         {"verbose", 'v', POPT_ARG_NONE,   NULL,                'V', "Show extended detail",        NULL},
         {"terse",   't', POPT_ARG_NONE,   NULL,                'T', "Show brief detail",           NULL},
         {"program", 'p', POPT_ARG_NONE,   NULL,                'P', "Machine readable output",     NULL},
 //      {"verbose", 'v', POPT_ARG_VAL,    &parsed_cmd->msg_level, VERBOSE, "Show extended detail",   NULL},
 //      {"terse",   't', POPT_ARG_VAL,    &parsed_cmd->msg_level, TERSE,   "Show brief detail",      NULL},
 //      {"program", 'p', POPT_ARG_NONE,   NULL,                'P', "Machine readable output",     NULL},
 //      {"verbose", 'v', POPT_ARG_VAL,    &parsed_cmd->output_level, OL_VERBOSE, "Show extended detail",   NULL},
 //      {"terse",   't', POPT_ARG_VAL,    &parsed_cmd->output_level, OL_TERSE,   "Show brief detail",      NULL},
 //       {"program", 'p', POPT_ARG_NONE,   &parsed_cmd->output_level, OL_PROGRAM, "Machine readable output",     NULL},
         {"force",   'f', POPT_ARG_NONE,   NULL,                'F', "Do not check certain parms",  NULL},
//       {"force",   'f', POPT_ARG_NONE,   &parsedCmd->force,   'F', "Do not check certain parms",  NULL},
         {"model",   'l', POPT_ARG_STRING, &modelwork,          'L', "Select monitor by model and serial number",  "model name"},
         {"sn",      'n', POPT_ARG_STRING, &snwork,             'N', "Select monitor by model and serial number",  "string serial number"},
         {"edidstr", 'e', POPT_ARG_STRING, &edidwork,           'E', "Select monitor by EDID", "128 byte EDID as 256 character hex string"},
         {"edid",    '\0',POPT_ARG_STRING, &edidwork,           'E', "Select monitor by EDID", "128 byte EDID as 256 character hex string"},
         {"trace",   'r', POPT_ARG_STRING, &tracework,          'R', "trace classes",   "comma separated list of trace classes, or all" },
         {"version", '\0',POPT_ARG_NONE,   NULL,                'Z', "Show version information"},
         POPT_AUTOHELP
         POPT_TABLEEND
      // { NULL, 0, 0, NULL, 0}
      // {NULL}
   };

   if (debug) {
      printf("(%s) argc=%d\n", __func__, argc);
      int ndx = 0;
      for (; ndx < argc; ndx++) {
         printf("(%s) argv[%d] = |%s|\n", __func__, ndx, argv[ndx]);
      }
   }

   poptContext pc = poptGetContext(NULL, argc, (const char **)  argv, po, 0);

   // comma delimited list of trace identifiers:
   char * trace_group_string = strjoin(trace_group_names, trace_group_ct, ", ");
   // printf("(%s) traceGroupString = %s\n", __func__, traceGroupString);


   char * other_option_help = "command [command args]\n\n"
               "  Commands:\n"
               "     detect\n"
               "     capabilities\n"
 //            "     info\n"
               "     listvcp\n"
               "     getvcp <feature-code>\n"
               "     setvcp <feature-code> <new-value>\n"
               "     dumpvcp (filename)\n"
               "     loadvcp <filename>\n"
               "     testcase <testcase-number>\n"
               "     listtests\n"
               "\n"
               "  The <feature-code> argument to getvcp can be any of the following:\n"
               "     - the hex feature code for a specific feature, with or without a leading 0x, e.g. 10 or 0x10\n"
               "     - ALL - all known feature codes\n"
               "     - COLORMGT - color related feature codes\n"
               "     - PROFILE - color related codes for profile management\n"
               "  Keywords ALL, COLORMGT, and PROFILE can be abbreviated to the first 3 characters.\n"
               "  Case is ignored.  e.g. \"COL\", \"pro\"\n"
               "\n"
               "  The monitor to be processed can be specified using any of the options:\n"
               "     --bus, --adl, --model and --sn, --edidstr\n"
               "\n"
               "  The argument to --trace is a comma separated list of trace classes,\n"
               "  or the keyword \"ALL\"\n"
               "\n"
               ;

   const char * pieces[] = {other_option_help, "  Recognized trace classes: ", trace_group_string, "\n\n"};
   other_option_help = strjoin(pieces, 4, NULL);
   poptSetOtherOptionHelp(pc,  other_option_help);
   // poptSetOtherOptionHelp(pc, "[ARG...]");

   if (argc < 2) {
       poptPrintUsage(pc, stderr, 0);
       exit(EXIT_FAILURE);
   }

   int explicit_display_spec_ct = 0;  // number of ways the display is explicitly specified
   bool ok = true;                    // set false if any error encountered
   // process options and handle each val returned
   int val;
   while ((val = poptGetNextOpt(pc)) >= 0) {
      if (debug)
         printf("(%s) poptGetNextOpt returned val %c (%d)\n", __func__, val, val);
      // printf("(%s) Top of loop, ok=%d\n", __func__, ok);
      switch (val) {
      case 'A':
         {
            // printf("(%s) case 'A', debug=%d\n", __func__, ok);
            if (debug)
               printf("(%s) case 'A', adlwork = |%s|\n", __func__, adlwork);
            int iAdapterIndex;
            int iDisplayIndex;
            bool adlok = parse_adl_arg(adlwork, &iAdapterIndex, &iDisplayIndex);
            if (!adlok) {
               // how to set POPT_ERROR_BADNUMBER?
               printf("Invalid ADL argument: %s\n", adlwork );
               ok = false;
               // printf("(%s) After ADL parse, ok=%d\n", __func__, ok);
            }
            else {
               // parsedCmd->dref = createAdlDisplayRef(iAdapterIndex, iDisplayIndex);
               free(parsed_cmd->pdid);
               parsed_cmd->pdid = create_adlno_display_identifier(iAdapterIndex, iDisplayIndex);  // new way
            }
            explicit_display_spec_ct++;
         }
         break;
      case 'B':
         // printf("(%s) case B\n", __func__);
         // parsedCmd->dref = createBusDisplayRef(buswork);
         free(parsed_cmd->pdid);
         parsed_cmd->pdid = create_busno_display_identifier(buswork);
         explicit_display_spec_ct++;
         break;
      case 'C':
         // printf("(%s) case C\n", __func__);
         parsed_cmd->ddcdata = true;
         break;
      case 'E':
         {
            if (strlen(edidwork) != 256) {
               printf("(%s) EDID hex string not 256 characters\n", __func__ );
               ok = false;
            }
            else {
               Byte * pba = NULL;
               int bytect = hhs_to_byte_array(edidwork, &pba);
               if (bytect < 0 || bytect != 128) {
                  printf("(%s) Invalid EDID hex string\n", __func__ );
                  ok = false;
               }
               else {
                  free(parsed_cmd->pdid);
                  parsed_cmd->pdid = create_ddid_display_identifier(pba);  // new way
               }
               if (pba)
                  free(pba);
            }
            explicit_display_spec_ct++;
         }
         break;
      case 'F':
         if (debug)
            printf("(%s) case 'F', value in parsedCmd->force = %d\n",
                   __func__, parsed_cmd->force);
         parsed_cmd->force = true;
         break;
      case 'L':
         if (debug)
            printf("(%s) case 'L', value in modelwork = |%s|\n", __func__, modelwork);
         break;
      case 'N':
         if (debug)
            printf("(%s) case 'N', value in snwork = |%s|\n", __func__, snwork);
         break;
      case 'P':
         // printf("(%s) case 'P', value in parsedCmd->programmatic_output = %d\n",
         //        __func__, parsed_cmd->programmatic_output);
#ifdef OLD
         parsed_cmd->programmatic_output = true;
#endif
         parsed_cmd->output_level = OL_PROGRAM;
         break;
      case 'S':
         if (debug)
            printf("(%s) case 'S', value in parsedCmd->stats = %d\n", __func__, parsed_cmd->stats);
         parsed_cmd->stats = true;
         break;
      case 'T':
         if (debug)
            printf("(%s) case 'T'\n", __func__ );
#ifdef OLD
         parsed_cmd->msg_level = TERSE;
#endif
         parsed_cmd->output_level = OL_TERSE;
         break;
      case 'V':
         if (debug)
            printf("(%s) case 'V'\n", __func__ );
#ifdef OLD
         parsed_cmd->msg_level = VERBOSE;
#endif
         parsed_cmd->output_level = OL_VERBOSE;
         break;
      case 'R':
         if (debug)
            printf("(%s) case 'R', argument = |%s|\n", __func__, tracework );
         strupper(tracework);
         {
            Trace_Group traceClasses = 0x00;
            if (strcmp(tracework, "ALL") == 0 ||
                strcmp(tracework, "*")   == 0) {
               // printf("(%s) Process ALL\n", __func__);
               traceClasses = 0xFF;
            }
            else {
               char * rest = tracework;
               char * token;
               char delim = ',';
               // originally token assignment was in while() clause, but valgrind
               // complaining about uninitialized variable, trying to figure out why
               token = strsep(&rest, &delim);
               while (token) {
                  // printf("(%s) token: |%s|\n", __func__, token);
                  Trace_Group tg = trace_class_name_to_value(token);
                  // printf("(%s) tg=0x%02x\n", __func__, tg);
                  if (tg) {
                     traceClasses |= tg;
                  }
                  else {
                     printf("Invalid trace group: %s\n", token);
                     ok = false;
                  }
                  token = strsep(&rest, &delim);
               }
            }
            // printf("(%s) traceClasses = 0x%02x\n", __func__, traceClasses);
            parsed_cmd->trace = traceClasses;
         }
         break;
      case 'Z':
         {
            printf("Compiled %s at %s\n", __DATE__, __TIME__ );
            exit(0);
         }
      default:
         printf("(%s) Unexpected poptGetNextOpt() value: %c(%d)\n", __func__, val, val);
         ok = false;
         break;
      }
   }

   // poptGetNextOpt returns -1 when the final argument has been parsed
   // otherwise an error occurred
   if (val != -1) {
        // handle error
      switch(val) {
         case POPT_ERROR_NOARG:
            printf("Argument missing for an option\n");
            break;
         case POPT_ERROR_BADOPT:
            printf("Option's argument could not be parsed\n");
            break;
         case POPT_ERROR_BADNUMBER:
         case POPT_ERROR_OVERFLOW:
            printf("Option could not be converted to number\n");
            break;
         case POPT_ERROR_BADOPERATION:
            printf("Mutually exclusive logical operations requested (POPT_ERROR_BADOPERATION)\n");
            break;
         default:
            printf("(%s) Unknown error in option processing. val=%d\n", __func__, val);
            break;
      }
      // exit(EXIT_FAILURE);
      ok = false;
   }


   if (modelwork && snwork) {
      free(parsed_cmd->pdid);
      parsed_cmd->pdid = create_mon_ser_display_identifier(modelwork, snwork);  // new way
      explicit_display_spec_ct++;
   }
   else if (modelwork || snwork) {
      puts("--model and --sn must be specified together");
      ok = false;
   }

   if (explicit_display_spec_ct > 1) {
      puts("Display has been specified in more than 1 way");
      ok = false;
   }

   if (poptPeekArg(pc) == NULL) {
      puts("No command specified.");
      ok = false;
   }
   else {
      char * cmd = (char *) poptGetArg(pc);
      if (debug)
         printf("cmd=|%s|\n", cmd);
      Cmd_Desc * cmdInfo = find_command(cmd);

      if (cmdInfo == NULL) {
         printf("Unrecognized command: %s\n", cmd);
         ok = false;
      }
      else {
         if (debug)
            show_cmd_desc(cmdInfo);
         // process command args
         parsed_cmd->cmd_id  = cmdInfo->cmd_id;
         // parsedCmd->argCt  = cmdInfo->argct;
         int min_arg_ct = cmdInfo->min_arg_ct;
         int max_arg_ct = cmdInfo->max_arg_ct;
         int argctr = 0;
         while ( poptPeekArg(pc) != NULL) {
            // printf("loop.  argctr=%d\n", argctr);
            if (argctr == max_arg_ct) {
               puts("Too many arguments");
               ok = false;
               break;
            }
            char * thisarg = (char *) poptGetArg(pc);
            // printf("thisarg |%s|\n", thisarg);
            char * argcopy = strdup(thisarg);
            parsed_cmd->args[argctr] = argcopy;
            argctr++;
         }
         parsed_cmd->argct = argctr;

         // no more arguments specified
         if (argctr < min_arg_ct) {
            puts("Missing argument(s)");
            ok = false;
         }
      }
   }

   // check that output_level consistent with cmd_id
   Byte valid_output_levels;
   Byte default_output_level = OL_NORMAL;
   switch(parsed_cmd->cmd_id) {
      case (CMDID_DETECT):
         valid_output_levels = OL_PROGRAM | OL_TERSE | OL_NORMAL | OL_VERBOSE;
         break;
      case (CMDID_GETVCP):
         valid_output_levels = OL_PROGRAM | OL_TERSE | OL_NORMAL | OL_VERBOSE;
         break;
      case (CMDID_DUMPVCP):
         valid_output_levels = OL_PROGRAM;
         default_output_level = OL_PROGRAM;
         break;
      default:
         default_output_level = OL_NORMAL;
         valid_output_levels = OL_TERSE | OL_NORMAL | OL_VERBOSE;
   }

   if (parsed_cmd->output_level == OL_DEFAULT)
         parsed_cmd->output_level = default_output_level;
   if (!(parsed_cmd->output_level & valid_output_levels)) {
      printf("Output level invalid for command %s: %s\n",
             get_command(parsed_cmd->cmd_id)->cmd_name,
             output_level_name(parsed_cmd->output_level) );
      ok = false;
   }

#ifdef OLD
   // *** TEMP ***
   // Tie back to old control mechanisms for now:
   parsed_cmd->programmatic_output = false;
   switch(parsed_cmd->output_level){
      case(OL_PROGRAM):
         parsed_cmd->msg_level = NORMAL;
         parsed_cmd->programmatic_output = true;
         break;
      case (OL_TERSE):
         parsed_cmd->msg_level = TERSE;
         break;
      case (OL_NORMAL):
         parsed_cmd->msg_level = NORMAL;
         break;
      case (OL_VERBOSE):
         parsed_cmd->msg_level = VERBOSE;
         break;
      default:
         PROGRAM_LOGIC_ERROR( "parsed_cmd->output_level = %d\n", parsed_cmd->output_level);
   }
#endif


   if (ok) {
      if (parsed_cmd->cmd_id == CMDID_SETVCP) {
         // perform checking here?
      }
   }

   if (ok && parsed_cmd->cmd_id == CMDID_TESTCASE) {
      if (parsed_cmd->pdid->id_type != DISP_ID_BUSNO && parsed_cmd->pdid->id_type != DISP_ID_ADL) {
         puts("testcase requires display specification using --bus or --adl");
         ok = false;
      }
   }

   if (debug) {
      printf("(%s) Done. ok=%d\n", __func__, ok);
      show_parsed_cmd(parsed_cmd);
   }

   if (!ok) {
      free_parsed_cmd(parsed_cmd);
      parsed_cmd = NULL;
   }

   poptFreeContext(pc);
   if (debug)
      printf("(%s) Returning: %p\n", __func__, parsed_cmd);
   return parsed_cmd;
}


void free_parsed_cmd(Parsed_Cmd * parsed_cmd) {
   assert ( memcmp(parsed_cmd->marker,PARSED_CMD_MARKER,4) == 0);
   int ndx = 0;
   for (; ndx < parsed_cmd->argct; ndx++)
      free(parsed_cmd->args[ndx]);
   if (parsed_cmd->pdid)
      free_display_identifier(parsed_cmd->pdid);
   parsed_cmd->marker[3] = 'x';
   free(parsed_cmd);
}

