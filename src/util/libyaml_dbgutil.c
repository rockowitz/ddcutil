/** \file libyaml_util.c
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <yaml.h>
 
#include "glib_string_util.h"
#include "data_structures.h"
#include "libyaml_dbgutil.h"
#include "report_util.h"


// Yaml_Mode is defined in "libyaml_util.h", not <yaml.h>
char * yaml_mode_name(Dbg_Yaml_Parse_Mode mode) {
   char * result = NULL;
   switch(mode) {
   case YAML_PARSE_TOKENS:   result="YAML_PARSE_STREAM";    break;
   case YAML_PARSE_EVENTS:   result="YAML_PARSE_EVENTS";    break;
   case YAML_PARSE_DOCUMENT: result="YAML_PARSE_DOCUMENT";  break;
   }
   return result;
}


Value_Name_Title_Table yaml_error_table = {
      VNT(YAML_NO_ERROR,       "No error is produced"                            ),
      VNT(YAML_MEMORY_ERROR,   "Cannot allocate or reallocate a block of memory" ),
      VNT(YAML_READER_ERROR,   "Cannot read or decode the input stream"           ),
      VNT(YAML_SCANNER_ERROR,  "Cannot scan the inptu stream"                    ),
      VNT(YAML_PARSER_ERROR,   "Cannot parse the input stream"                   ),
      VNT(YAML_COMPOSER_ERROR, "Cannot compose a YAML document"                  ),
      VNT(YAML_WRITER_ERROR,   "Cannot write to IO stream"                       ),
      VNT(YAML_EMITTER_ERROR,  "Cannot emit a YAML stream"                       ),
      VNT_END
};

char * yaml_error_name(yaml_error_type_t error) {
 return     vnt_name(yaml_error_table, error);
}


Value_Name_Table node_type_table = {
      VN(YAML_NO_NODE),
      VN(YAML_SCALAR_NODE),
      VN(YAML_SEQUENCE_NODE),
      VN(YAML_MAPPING_NODE),
      VN_END
};

char *  yaml_node_type_name(yaml_node_type_t type) {
   return vnt_name(node_type_table, type);
}


Value_Name_Table scalar_style_table = {
      VN(YAML_ANY_SCALAR_STYLE),
      VN(YAML_PLAIN_SCALAR_STYLE),
      VN(YAML_SINGLE_QUOTED_SCALAR_STYLE),
      VN(YAML_DOUBLE_QUOTED_SCALAR_STYLE),
      VN(YAML_LITERAL_SCALAR_STYLE),
      VN(YAML_FOLDED_SCALAR_STYLE),
      VN_END
};

char * yaml_scalar_style_name(yaml_scalar_style_t style) {
   return vnt_name(scalar_style_table, style);
}

Value_Name_Table mapping_style_table = {
      VN(YAML_ANY_MAPPING_STYLE),
      VN(YAML_BLOCK_MAPPING_STYLE),
      VN(YAML_FLOW_MAPPING_STYLE),
      VN_END
};

char * yaml_mapping_style_name(yaml_mapping_style_t style) {
   return vnt_name(mapping_style_table, style);
}

Value_Name_Table sequence_style_table = {
      VN(YAML_ANY_SEQUENCE_STYLE),
      VN(YAML_BLOCK_SEQUENCE_STYLE),
      VN(YAML_FLOW_SEQUENCE_STYLE),
      VN_END
};

char * yaml_sequence_style_name(yaml_sequence_style_t style) {
   return vnt_name(sequence_style_table, style);
};

#ifdef REF
/** The tag directive data. */
typedef struct yaml_tag_directive_s {
    /** The tag handle. */
    yaml_char_t *handle;
    /** The tag prefix. */
    yaml_char_t *prefix;
} yaml_tag_directive_t;
#endif

void dbgrpt_yaml_tag_directive(yaml_tag_directive_t* directive, char * msg, int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "yaml_tag_directive_t at %p", directive);
   if (msg)
      rpt_label(depth,msg);
   rpt_vstring(d1, "handle: %s", directive->handle);
   rpt_vstring(d1, "prefix: %s", directive->prefix);
}




void dbgrpt_yaml_mark(yaml_mark_t mark, char * msg, int depth) {
   int d0 = depth;
   int d1 = depth+1;
   if (msg)
      rpt_vstring(d0, msg);
   rpt_vstring(d1, "index:   %d", mark.index);
   rpt_vstring(d1, "line:    %d", mark.line);
   rpt_vstring(d1, "column   %d", mark.column);
}


// void dbgrpt_yaml_scalar(yaml_scalar_type_t sc, int depth) {
// }

void dbgrpt_yaml_token(yaml_token_t token, int depth) {
}


void dbgrpt_yaml_scalar_event(yaml_event_t* event, int depth)
{
   int d0 = depth;
   int d1 = depth+1;
   assert(event->type == YAML_SCALAR_EVENT);
   rpt_vstring(d0, "YAML_SCALAR_EVENT");
   rpt_vstring(d1, "tag:             %s", event->data.scalar.tag);
   rpt_vstring(d1, "value:           %s", event->data.scalar.value);
   rpt_vstring(d1, "length:          %d", event->data.scalar.length);
   rpt_vstring(d1, "plain_implicit:  %d", event->data.scalar.plain_implicit);
   rpt_vstring(d1, "quoted_implicit: %d", event->data.scalar.quoted_implicit);
   rpt_vstring(d1, "scalar style:    %s",  yaml_scalar_style_name(event->data.scalar.style));
}


#ifdef OLD
void dbgrpt_yaml_no_node(yaml_node_t * node, int depth) {
   int d0 = depth;
   int d1 = depth+1;
   rpt_vstring(d0, "node of type YAML_NO_NODE at %p", node);
   assert(node->type == YAML_NO_NODE);

   dbgrpt_yaml_mark(node->start_mark, "start_mark:", d1);
   dbgrpt_yaml_mark(node->end_mark,   "end mark:  ", d1);
}
#endif

#ifdef OLD
void dbgrpt_yaml_scalar_node(yaml_node_t * node, int depth) {
   int d0 = depth;
   int d1 = depth+1;
   rpt_vstring(d0, "scalar node at %p", node);
   assert(node->type == YAML_SCALAR_NODE);
   rpt_vstring(d1, "value:           %s", node->data.scalar.value);
   rpt_vstring(d1, "length:          %d", node->data.scalar.length);
   rpt_vstring(d1, "scalar style:    %s",  yaml_scalar_style_name(node->data.scalar.style));
   dbgrpt_yaml_mark(node->start_mark, "start_mark:", d1);
   dbgrpt_yaml_mark(node->end_mark,   "end mark:  ", d1);
}
#endif


#ifdef REF
/** An element of a mapping node. */
typedef struct yaml_node_pair_s {
    /** The key of the element. */
    int key;
    /** The value of the element. */
    int value;
} yaml_node_pair_t;
#endif


void dbgrpt_yaml_node_pair(yaml_node_pair_t* pair, char * msg, int depth) {
   int d0 = depth;
   int d1 = depth+1;
   if (msg)
      rpt_label(d0, msg);
   rpt_vstring(d0, "yaml_node_pair at %p", pair);
   rpt_vstring(d1, "key:     %d", pair->key);
   rpt_vstring(d1, "value:   %d", pair->value);
}


#ifdef REF
/** The mapping parameters (for @c YAML_MAPPING_NODE). */
 struct {
     /** The stack of mapping pairs (key, value). */
     struct {
         /** The beginning of the stack. */
         yaml_node_pair_t *start;
         /** The end of the stack. */
         yaml_node_pair_t *end;
         /** The top of the stack. */
         yaml_node_pair_t *top;
     } pairs;
     /** The mapping style. */
     yaml_mapping_style_t style;
 } mapping;
#endif

#ifdef OLD
void dbgrpt_yaml_mapping_node(yaml_node_t * node, char * msg, int depth) {
   int d1 = depth+1;
   if (msg)
      rpt_label(depth, msg);
   rpt_vstring(depth, "yaml mapping node @ %p", node);
   assert(node->type == YAML_MAPPING_NODE);
   rpt_vstring(d1, "pairs.start  %p", node->data.mapping.pairs.start);
   rpt_vstring(d1, "pairs.end  %p", node->data.mapping.pairs.end);
   rpt_vstring(d1, "pairs.top  %p", node->data.mapping.pairs.top);
   for (yaml_node_pair_t* pair =  node->data.mapping.pairs.start;
         pair < node->data.mapping.pairs.top; pair++)
   {
      dbgrpt_yaml_node_pair(pair, NULL, d1);
   }

   // dbgrpt_yaml_node_pair(node->data.mapping.pairs.start, "start", d1);
   // dbgrpt_yaml_node_pair(node->data.mapping.pairs.end,   "end", d1);
   // dbgrpt_yaml_node_pair(node->data.mapping.pairs.top,   "top", d1);
   rpt_vstring(d1, "mapping style:    %s",  yaml_mapping_style_name(node->data.mapping.style));
   dbgrpt_yaml_mark(node->start_mark, "start_mark:", d1);
   dbgrpt_yaml_mark(node->end_mark,   "end mark:  ", d1);
}


void dbgrpt_yaml_sequence_node(yaml_node_t * node, char * msg, int depth) {
      int d1 = depth+1;
      if (msg)
         rpt_label(depth, msg);
      rpt_vstring(depth, "yaml sequence node @ %p", node);
      assert(node->type == YAML_SEQUENCE_NODE);
      rpt_label(d1, "UNIMPLEMENTED");
}

#endif

void dbgrpt_yaml_node(yaml_document_t * document, yaml_node_t * node, char * msg, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   int d3 = depth+3;
   if (msg)
      rpt_vstring(depth, msg);
   rpt_vstring(depth, "yaml node @ %p", node);
   rpt_vstring(d1, "type:      %d=%s", node->type, yaml_node_type_name(node->type));
   rpt_vstring(d1, "tag addr: %p", node->tag);
   if (node->type != YAML_NO_NODE)
   // if (node->tag)
      rpt_vstring(d1, "tag:       %s", node->tag);
   switch(node->type) {
      case YAML_NO_NODE:
         break;
      case YAML_SCALAR_NODE:
         rpt_vstring(d1, "value:           %s", node->data.scalar.value);
         rpt_vstring(d1, "length:          %d", node->data.scalar.length);
         rpt_vstring(d1, "scalar style:    %s",  yaml_scalar_style_name(node->data.scalar.style));
         break;
      case YAML_MAPPING_NODE:
         rpt_vstring(d1, "pairs.start  %p", node->data.mapping.pairs.start);
         rpt_vstring(d1, "pairs.end  %p", node->data.mapping.pairs.end);
         rpt_vstring(d1, "pairs.top  %p", node->data.mapping.pairs.top);
         for (yaml_node_pair_t* pair =  node->data.mapping.pairs.start;
               pair < node->data.mapping.pairs.top; pair++)
         {
            dbgrpt_yaml_node_pair(pair, NULL, d1);
            // int indexKey = pair->key;
            // int valueKey = pair->value;
            rpt_vstring(d2, "key node:");
            yaml_node_t * key_node = yaml_document_get_node(document,  pair->key);
            dbgrpt_yaml_node(document, key_node, "key node", d3);
            rpt_vstring(d2, "value node:");
            yaml_node_t * value_node = yaml_document_get_node(document,  pair->value);
            dbgrpt_yaml_node(document, value_node, "value node", d3);

         }

         // dbgrpt_yaml_node_pair(node->data.mapping.pairs.start, "start", d1);
         // dbgrpt_yaml_node_pair(node->data.mapping.pairs.end,   "end", d1);
         // dbgrpt_yaml_node_pair(node->data.mapping.pairs.top,   "top", d1);
         rpt_vstring(d1, "mapping style:    %s",  yaml_mapping_style_name(node->data.mapping.style));
         break;
      case YAML_SEQUENCE_NODE:
#ifdef REF
         /** The sequence parameters (for @c YAML_SEQUENCE_NODE). */
         struct {
             /** The stack of sequence items. */
             struct {
                 /** The beginning of the stack. */
                 yaml_node_item_t *start;
                 /** The end of the stack. */
                 yaml_node_item_t *end;
                 /** The top of the stack. */
                 yaml_node_item_t *top;
             } items;
             /** The sequence style. */
             yaml_sequence_style_t style;
         } sequence;
#endif
         rpt_vstring(d1, "sequence.items.start  %p", node->data.sequence.items.start);
         rpt_vstring(d1, "sequence.items.end    %p", node->data.sequence.items.end);
         rpt_vstring(d1, "sequence.items        %p", node->data.sequence.items.top);
         for (yaml_node_item_t * item =  node->data.sequence.items.start;
               item < node->data.sequence.items.top;
               item++)
         {
            rpt_vstring(d2, "item index = %d",  *item);
            yaml_node_t * item_node = yaml_document_get_node(document, *item);
            dbgrpt_yaml_node(document, item_node, "item node", d3);
         }
         break;
      default:
         rpt_vstring(d2, "Unimplemented"); break;
   };
   rpt_vstring(d1, "start_mark = %p", &node->start_mark);
   rpt_vstring(d1, "end_mark   = %p", &node->end_mark);
   dbgrpt_yaml_mark(node->start_mark, "start_mark:", d1);
   dbgrpt_yaml_mark(node->end_mark,   "end mark:  ", d1);
   rpt_vstring(d1, "node done");
}


void dbgrpt_yaml_tokens(yaml_parser_t* parser, int depth) {
   rpt_label(depth, "(dbgrpt_yaml_tokens) Unimplemented");
}


void dbgrpt_yaml_events(yaml_parser_t* parser, int depth) {
   int d0 = depth;
   int d1 = depth+1;

   yaml_event_t event;

   do {
        if (!yaml_parser_parse(parser, &event)) {
           rpt_vstring(d1, "Parser error %d", parser->error);
           return;
        }

        switch(event.type)
        {
           case YAML_NO_EVENT: rpt_label(d0,"No event!"); break;
           /* Stream start/end */
           case YAML_STREAM_START_EVENT: rpt_label(d0,"STREAM START"); break;
           case YAML_STREAM_END_EVENT:   rpt_label(d0,"STREAM END");   break;
           /* Block delimeters */
           case YAML_DOCUMENT_START_EVENT: rpt_label(d0,"<b>Start Document</b>"); break;
           case YAML_DOCUMENT_END_EVENT:   rpt_label(d0,"<b>End Document</b>");   break;
           case YAML_SEQUENCE_START_EVENT: rpt_label(d0,"<b>Start Sequence</b>"); break;
           case YAML_SEQUENCE_END_EVENT:   rpt_label(d0,"<b>End Sequence</b>");   break;
           case YAML_MAPPING_START_EVENT:  rpt_label(d0,"<b>Start Mapping</b>");  break;
           case YAML_MAPPING_END_EVENT:    rpt_label(d0,"<b>End Mapping</b>");    break;
           /* Data */
           case YAML_ALIAS_EVENT:  printf("Got alias (anchor %s)\n", event.data.alias.anchor); break;
           case YAML_SCALAR_EVENT:
              printf("Got scalar (value %s)\n", event.data.scalar.value);
              dbgrpt_yaml_scalar_event(&event, d1);
              break;
           }
           if(event.type != YAML_STREAM_END_EVENT)
             yaml_event_delete(&event);
      } while(event.type != YAML_STREAM_END_EVENT);
      yaml_event_delete(&event);
      /* END new code */
}


#ifdef REF
/** The document structure. */
typedef struct yaml_document_s {

    /** The document nodes. */
    struct {
        /** The beginning of the stack. */
        yaml_node_t *start;
        /** The end of the stack. */
        yaml_node_t *end;
        /** The top of the stack. */
        yaml_node_t *top;
    } nodes;

    /** The version directive. */
    yaml_version_directive_t *version_directive;

    /** The list of tag directives. */
    struct {
        /** The beginning of the tag directives list. */
        yaml_tag_directive_t *start;
        /** The end of the tag directives list. */
        yaml_tag_directive_t *end;
    } tag_directives;

    /** Is the document start indicator implicit? */
    int start_implicit;
    /** Is the document end indicator implicit? */
    int end_implicit;

    /** The beginning of the document. */
    yaml_mark_t start_mark;
    /** The end of the document. */
    yaml_mark_t end_mark;

} yaml_document_t;
#endif






void dbgrpt_yaml_document(yaml_document_t * document, int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "yaml document at %p", document);
   for (yaml_node_t* node = document->nodes.start;
        node < document->nodes.top;
        node++)
   {
      dbgrpt_yaml_node( document, node, "document node", d1);
   }
   // dbgrpt_yaml_node(document->nodes.start, "nodes.start:", d1);
   // dbgrpt_yaml_node(document->nodes.end, "nodes.end::", d1);
   // dbgrpt_yaml_node(document->nodes.top, "nodes.top:", d1);
   rpt_vstring(d1, "document->version_directive = %p", document->version_directive);
   if (document->version_directive)
      rpt_vstring(d1, "version_directive: %d.%d", document->version_directive->major, document->version_directive->minor);

   for (yaml_tag_directive_t* directive = document->tag_directives.start;
        directive < document->tag_directives.end;
        directive++)
   {
      dbgrpt_yaml_tag_directive(directive, "document tag directive", d1);
   }

   // dbgrpt_yaml_tag_directive(document->tag_directives.start, "tag_directives.start:", d1);
   // dbgrpt_yaml_tag_directive(document->tag_directives.end, "tag_directives.end:", d1);
   rpt_vstring(d1, "start_implicit = %d, end_implicit = %d", document->start_implicit, document->end_implicit);
   dbgrpt_yaml_mark(document->start_mark, "start mark:", d1);
   dbgrpt_yaml_mark(document->end_mark, "end mark: ", d1);
}


void dbgrpt_yaml_document_main(yaml_parser_t *parser, int depth) {
   int d0 = depth;
   int d1 = depth+1;

   yaml_document_t document;

   if (!yaml_parser_load(parser, &document) ) {
      rpt_label(d0, "yaml_parser_load() failed");
      return;
   }

   dbgrpt_yaml_document(&document, d1);

   yaml_node_t * cur_node = yaml_document_get_root_node(&document);
   assert(cur_node);
   dbgrpt_yaml_node(&document, cur_node,NULL, d1);


#ifdef NO

   while(true) {
      node = yaml_document_get_node(&document, node_number);
      rpt_vstring(d0, "Node [%d]: %d", node_number++, node->type);
      if (node->type == YAML_SCALAR_NODE) {
         rpt_vstring(d1, "Scalar [%d] =  %s",
               node->data.scalar.style, node->data.scalar.value);
      }
   }

#endif
   yaml_document_delete(&document);
}


void dbgrpt_yaml_by_file_handle(
      FILE *               fh,
      Dbg_Yaml_Parse_Mode  mode,
      int                  depth)
{
     int d0 = depth;
     int d1 = depth+1;

     rpt_vstring(d0, "Reporting file as %s", yaml_mode_name(mode));
     if (!fh) {
        rpt_label(d1,"Null file handle\n");
        return;
     }
     rewind(fh);

     yaml_parser_t parser;

     /* Initialize parser */
     if(!yaml_parser_initialize(&parser)) {
       rpt_label(d1,"Failed to initialize parser!");
       return;
     }

     /* Set input file */
     yaml_parser_set_input_file(&parser, fh);

     switch(mode) {
     case YAML_PARSE_TOKENS:
        dbgrpt_yaml_tokens(&parser, d1); break;
     case YAML_PARSE_EVENTS:
        dbgrpt_yaml_events(&parser, d1); break;
     case YAML_PARSE_DOCUMENT:
        dbgrpt_yaml_document_main(&parser, d1); break;
     }

     /* Cleanup */
     yaml_parser_delete(&parser);
}


void dbgrpt_yaml_by_filename(
      const char *        filename,
      Dbg_Yaml_Parse_Mode  mode,
      int                 depth)
{
   FILE * fh = fopen(filename,"r");
   if (!fh) {
      rpt_vstring(depth, "Unable to open %s, errnno=%d", filename, errno);
      return;
   }

   dbgrpt_yaml_by_file_handle(fh, mode, depth);

   fclose(fh);
}


void dbgrpt_yaml_by_string(
      const char *         string,
      Dbg_Yaml_Parse_Mode  mode,
      int                  depth)
{
     int d0 = depth;
     int d1 = depth+1;

     rpt_vstring(d0, "Reporting yaml string as %s", yaml_mode_name(mode));

     yaml_parser_t parser;

     /* Initialize parser */
     if(!yaml_parser_initialize(&parser)) {
       rpt_label(d1,"Failed to initialize parser!");
       return;
     }

     /* Set input file */
     yaml_parser_set_input_string(&parser, (yaml_char_t *) string, strlen(string));

     switch(mode) {
     case YAML_PARSE_TOKENS:
        dbgrpt_yaml_tokens(&parser, d1); break;
     case YAML_PARSE_EVENTS:
        dbgrpt_yaml_events(&parser, d1); break;
     case YAML_PARSE_DOCUMENT:
        dbgrpt_yaml_document_main(&parser, d1); break;
     }

     /* Cleanup */
     yaml_parser_delete(&parser);
}

// move to string_util?
static char * join_ntsa_with_sepchar(char** ntsa, char sepchar) {
   int total_sz = 0;
   char * ptr = *ntsa;
   while (ptr != NULL) {
      total_sz += (strlen(ptr) + 1);
      ptr++;
   }

   char * buf = calloc(1, total_sz);
   char * endpos = buf;
   ptr = *ntsa;
   while (ptr != NULL) {
      memcpy(endpos, ptr, strlen(ptr));
      endpos += strlen(ptr);
      *(endpos++) = sepchar;
   }
   assert( (endpos - buf) == total_sz);
   *(endpos-1) = '\0';
   return buf;
}


void dbgrpt_yaml_by_lines(
      char * *            ntsa,
      Dbg_Yaml_Parse_Mode mode,
      int                 depth)
{
   char * buf = join_ntsa_with_sepchar(ntsa, '\n');
   dbgrpt_yaml_by_string(buf, mode, depth);
   free(buf);
}


