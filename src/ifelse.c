//------------------------------------------------------------------------|
// Copyright (c) 2023-2024 by Raymond M. Foulk IV (rfoulk@gmail.com)
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
#include "ifelse.h"

//------------------------------------------------------------------------|
typedef struct
{
    // The raw unevaluated conditional expression associated with this
    bytes_t * condition;

    // Raw command lines consisting of the if-else blocks
    chain_t * if_lines;
    chain_t * else_lines;

    // which list of lines is being added to
    chain_t * lines;
}
scallop_ifelse_priv_t;

//------------------------------------------------------------------------|
static scallop_ifelse_t * scallop_ifelse_create(const char * condition)
{
    scallop_ifelse_t * ifelse = (scallop_ifelse_t *) malloc(
            sizeof(scallop_ifelse_t));
    if (!ifelse)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_ifelse_t)) failed");
        return NULL;
    }

    memcpy(ifelse, &scallop_ifelse_pub, sizeof(scallop_ifelse_t));

    ifelse->priv = malloc(sizeof(scallop_ifelse_priv_t));
    if (!ifelse->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_ifelse_priv_t)) failed");
        free(ifelse);
        return NULL;
    }

    memzero(ifelse->priv, sizeof(scallop_ifelse_priv_t));
    scallop_ifelse_priv_t * priv = (scallop_ifelse_priv_t *) ifelse->priv;

    // The conditional expression associated with the while loop
    // that should be re-evaluated on each iteration.
    priv->condition = bytes_pub.create(condition, strlen(condition));
    if (!priv->condition)
    {
        BLAMMO(FATAL, "bytes_pub.create(%s) failed", condition);
        ifelse->destroy(ifelse);
        return NULL;
    }

    // List of raw (mostly) uninterpreted command lines consisting
    // of the body of the ifelse.  One exception to this is we'll
    // need to track the nested depth of an 'end' keyword (multi-use)
    priv->if_lines = chain_pub.create(bytes_pub.copy,
                                     bytes_pub.destroy);
    if (!priv->if_lines)
    {
        BLAMMO(FATAL, "chain_pub.create() failed");
        ifelse->destroy(ifelse);
        return NULL;
    }

    priv->else_lines = chain_pub.create(bytes_pub.copy,
                                       bytes_pub.destroy);
    if (!priv->else_lines)
    {
        BLAMMO(FATAL, "chain_pub.create() failed");
        ifelse->destroy(ifelse);
        return NULL;
    }

    // Lines is initially pointed at if_lines
    priv->lines = priv->if_lines;

    ifelse->which_lines(ifelse, true);

    return ifelse;
}

//------------------------------------------------------------------------|
static void scallop_ifelse_destroy(void * ifelse_ptr)
{
    scallop_ifelse_t * ifelse = (scallop_ifelse_t *) ifelse_ptr;
    if (!ifelse || !ifelse->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    scallop_ifelse_priv_t * priv = (scallop_ifelse_priv_t *) ifelse->priv;

    if (priv->else_lines)
    {
        priv->else_lines->destroy(priv->else_lines);
    }

    if (priv->if_lines)
    {
        priv->if_lines->destroy(priv->if_lines);
    }

    if (priv->condition)
    {
        priv->condition->destroy(priv->condition);
    }

    memzero(ifelse->priv, sizeof(scallop_ifelse_priv_t));
    free(ifelse->priv);

    // zero out and destroy the public interface
    memzero(ifelse, sizeof(scallop_ifelse_t));
    free(ifelse);
}

//------------------------------------------------------------------------|
static void scallop_ifelse_which_lines(scallop_ifelse_t * ifelse,
                                       bool which)
{
    scallop_ifelse_priv_t * priv = (scallop_ifelse_priv_t *) ifelse->priv;

    if (which)
    {
        priv->lines = priv->if_lines;
    }
    else
    {
        priv->lines = priv->else_lines;
    }
}

//------------------------------------------------------------------------|
static void scallop_ifelse_append(scallop_ifelse_t * ifelse,
                                  const char * line)
{
    scallop_ifelse_priv_t * priv = (scallop_ifelse_priv_t *) ifelse->priv;

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
static int scallop_ifelse_runner(scallop_ifelse_t * ifelse,
                                void * context)
{
    scallop_ifelse_priv_t * priv = (scallop_ifelse_priv_t *) ifelse->priv;
    scallop_t * scallop = (scallop_t *) context;
    int result = 0;

    // Need to perform substitution and evaluation on each iteration
    if (scallop->evaluate_condition(scallop,
                                    priv->condition->cstr(priv->condition),
                                    priv->condition->size(priv->condition)))
    {
        // Iterate through all lines and dispatch each
        result = scallop->run_lines(scallop, priv->if_lines);
    }
    else
    {
        result = scallop->run_lines(scallop, priv->else_lines);
    }

    return result;
}

//------------------------------------------------------------------------|
const scallop_ifelse_t scallop_ifelse_pub = {
    &scallop_ifelse_create,
    &scallop_ifelse_destroy,
    &scallop_ifelse_which_lines,
    &scallop_ifelse_append,
    &scallop_ifelse_runner,
    NULL
};
