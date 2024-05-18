//------------------------------------------------------------------------|
// Copyright (c) 2023 by Raymond M. Foulk IV (rfoulk@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//------------------------------------------------------------------------|

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "iparser.h"
#include "console.h"
#include "command.h"
#include "routine.h"

//------------------------------------------------------------------------|
// Arbitrary maximum recursion depth to avoid stack smashing
#define SCALLOP_MAX_RECURS        64

//------------------------------------------------------------------------|
// Language construct line handler function signature
typedef int (*scallop_construct_line_f)(void * context,
                                        void * object,
                                        const char * line);

// Language construct stack pop handler function signature.
typedef int (*scallop_construct_pop_f)(void * context,
                                       void * object);

// Callback for registration of default commands on scallop->create().
// Normally one would pass in register_builtin_commands() to get all the
// default functionality.  Alternatively one could create something
// entirely different and just use the scallop engine.
typedef bool (*scallop_registration_f)(void * scallop);

//------------------------------------------------------------------------|
typedef struct scallop_t
{
    struct scallop_t * (*create)(console_t * console,
                                 scallop_registration_f registration,
                                 const char * prompt_base);

    // Scallop destructor function
    void (*destroy)(void * scallop);

    // Get access to the console object
    console_t * (*console)(struct scallop_t * scallop);

    // Get access to the command registry interface.
    // This is necessary for third-party command registration!
    scallop_cmd_t * (*commands)(struct scallop_t * scallop);

    // TODO: BEGIN: MOVE THESE INTO BUTTER PLUGIN
    // Get a routine by name.  Returns NULL if the routine is not found.
    scallop_rtn_t * (*routine_by_name)(struct scallop_t * scallop,
                                       const char * name);

    // Create and add a new routine to the internal list
    scallop_rtn_t * (*routine_insert)(struct scallop_t * scallop,
                                      const char * name);

    // Remove routine from the internal list
    void (*routine_remove)(struct scallop_t * scallop,
                           const char * name);
    // TODO: END: MOVE THESE INTO BUTTER PLUGIN

    // Add a dynamic or static plugin to Scallop
    bool (*plugin_add)(struct scallop_t * scallop,
                       const char * name,
                       scallop_registration_f addfunc,
                       scallop_registration_f rmfunc);

    // Remove the plugin from Scallop
    bool (*plugin_remove)(struct scallop_t * scallop,
                          const char * name);

    // Put a set of routine arguments into the environment to be
    // picked up later on evaluation/substitution.  This will
    // intrinsically prefix everything to avoid trampling on other
    // unrelated environment variables.
    void (*store_args)(struct scallop_t * scallop,
                       int argc,
                       char ** args);

    // Assign a variable value to scallop's environment
    void (*assign_variable)(struct scallop_t * scallop,
                            const char * varname,
                            const char * varvalue);

    // Evaluate a conditional expression, including variable references,
    // as with a while loop or if-else construct.
    // ex: "while ({i} < 3)" or "if ({x} == 5)"
    long (*evaluate_condition)(struct scallop_t * scallop,
                               const char * condition,
                               size_t size);

    // Get access to the internal parser object.
    // FIXME: This may ultimately be a stopgap measure while
    //  migrating to a unified "evaluate_condition"/expression
    //  call -- but this will require some refactoring.  consider
    //  removing this export ASAP.
    iparser_t * (*parser)(struct scallop_t * scallop);

    // Handle a raw line of input, calling whatever
    // handler functions are necessary.
    void (*dispatch)(struct scallop_t * scallop, const char * line);

    // Main interactive prompt loop: for console or source file
    int (*run_console)(struct scallop_t * scallop, bool interactive);

    // Run a given set of lines (must be a chain_t * type) as from
    // a routine or part of a while loop or if-else statement.
    int (*run_lines)(struct scallop_t * scallop, void * lines);

    // Explicitly quit the main loop
    void (*quit)(struct scallop_t * scallop);

    // Push a full context onto the context stack
    void (*construct_push)(struct scallop_t * scallop,
                         const char * name,
                         void * context,
                         void * object,
                         scallop_construct_line_f linefunc,
                         scallop_construct_pop_f popfunc);

    // Pop a context name off the top of the context stack
    int (*construct_pop)(struct scallop_t * scallop);

    // Get the bottom object out of the construct stack,
    // as this represents the current construct declaration
    void * (*construct_object)(struct scallop_t * scallop);

    // Private data
    void * priv;
}
scallop_t;

//------------------------------------------------------------------------|
// Public scallop interface
extern const scallop_t scallop_pub;
