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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

// RayCO
#include "utils.h"              // memzero()
#include "blammo.h"
#include "console.h"
#include "chain.h"
#include "bytes.h"

// Scallop
#include "scallop.h"
#include "command.h"
#include "routine.h"

//------------------------------------------------------------------------|
typedef struct
{
    // The name of this routine
    bytes_t * name;

    // Raw command lines consisting of the routine body
    chain_t * lines;
}
scallop_rtn_priv_t;

//------------------------------------------------------------------------|
static scallop_rtn_t * scallop_rtn_create(const char * name)
{
    OBJECT_ALLOC(scallop_, rtn);

    // Name of this routine (NO SPACES!!! - FIXME filter this)
    priv->name = bytes_pub.create(name, strlen(name));
    if (!priv->name)
    {
        BLAMMO(FATAL, "bytes_pub.create(%s) failed", name);
        rtn->destroy(rtn);
        return NULL;
    }

    // List of raw (mostly) uninterpreted command lines consisting
    // of the body of the routine.  One exception to this is we'll
    // need to track the nested depth of an 'end' keyword (multi-use)
    priv->lines = chain_pub.create(bytes_pub.copy,
                                   bytes_pub.destroy);
    if (!priv->lines)
    {
        BLAMMO(FATAL, "chain_pub.create() failed");
        rtn->destroy(rtn);
        return NULL;
    }

    return rtn;
}

//------------------------------------------------------------------------|
static void scallop_rtn_destroy(void * rtn_ptr)
{
    OBJECT_PTR(scallop_, rtn, rtn_ptr, );

    if (priv->lines)
    {
        priv->lines->destroy(priv->lines);
    }

    if (priv->name)
    {
        priv->name->destroy(priv->name);
    }

    OBJECT_FREE(scallop_, rtn);
}

static int scallop_rtn_compare_name(const void * rtn_ptr,
                                    const void * other)
{
    scallop_rtn_t * rtn1 = (scallop_rtn_t *) rtn_ptr;
    scallop_rtn_priv_t * priv1 = (scallop_rtn_priv_t *) rtn1->priv;

    scallop_rtn_t * rtn2 = (scallop_rtn_t *) other;
    scallop_rtn_priv_t * priv2 = (scallop_rtn_priv_t *) rtn2->priv;

    return priv1->name->compare(priv1->name, priv2->name);
}
//------------------------------------------------------------------------|
static inline const char * scallop_rtn_name(scallop_rtn_t * rtn)
{
    OBJECT_PRIV(scallop_, rtn);
    return priv->name->cstr(priv->name);
}

//------------------------------------------------------------------------|
static void scallop_rtn_append(scallop_rtn_t * rtn, const char * line)
{
    OBJECT_PRIV(scallop_, rtn);

    // First create the line object
    bytes_t * linebytes = bytes_pub.create(line, strlen(line));

    // Make no assumptions about the state of the chain, but always
    // force the insert at the 'end' of the chain, which is always at -1
    // since the chain is circular.
    priv->lines->reset(priv->lines);
    priv->lines->spin(priv->lines, -1);
    priv->lines->insert(priv->lines, linebytes);
}

//------------------------------------------------------------------------|
static int scallop_rtn_handler(void * scmd,
                               void * context,
                               int argc,
                               char ** args)
{
    // The command handler function for every routine once registered.
    // get the routine by name, and iterate through the lines calling
    // dispatch on each one until running out and then return.
    // A routine is very similar to a script.
    // args[0] happens to be the name of the routine,
    // But it is more reliably also cmd->name.
    scallop_cmd_t * cmd = (scallop_cmd_t *) scmd;
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    // Find _this_ routine.  TODO: Is there a faster way to
    // find/get the routine associated with the command we're
    // executing?  seems like there should be.  revisit this later.
    scallop_rtn_t * rtn = scallop->routine_by_name(scallop,
                                                   cmd->keyword(cmd));
    if (!rtn)
    {
        console->error(console,
                       "routine \'%s\' not found",
                       cmd->keyword(cmd));
        return -1;
    }

    // Store subroutine arguments in scallop's variable
    // collection so dispatch can perform substitution.
    scallop->store_args(scallop, argc, args);

    OBJECT_PRIV(scallop_, rtn);

    return scallop->run_lines(scallop, priv->lines);
}

//------------------------------------------------------------------------|
const scallop_rtn_t scallop_rtn_pub = {
    &scallop_rtn_create,
    &scallop_rtn_destroy,
    &scallop_rtn_compare_name,
    &scallop_rtn_name,
    &scallop_rtn_append,
    &scallop_rtn_handler,
    NULL
};
