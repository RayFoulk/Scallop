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
#include "while.h"

//------------------------------------------------------------------------|
typedef struct
{
    // The raw unevaluated conditional expression associated with this
    bytes_t * condition;

    // Raw command lines consisting of the while body
    chain_t * lines;
}
scallop_while_priv_t;

//------------------------------------------------------------------------|
static scallop_while_t * scallop_while_create(const char * condition)
{
    scallop_while_t * whileloop = (scallop_while_t *) malloc(
            sizeof(scallop_while_t));
    if (!whileloop)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_while_t)) failed");
        return NULL;
    }

    memcpy(whileloop, &scallop_while_pub, sizeof(scallop_while_t));

    whileloop->priv = malloc(sizeof(scallop_while_priv_t));
    if (!whileloop->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_while_priv_t)) failed");
        free(whileloop);
        return NULL;
    }

    memzero(whileloop->priv, sizeof(scallop_while_priv_t));
    scallop_while_priv_t * priv = (scallop_while_priv_t *) whileloop->priv;

    // The conditional expression associated with the while loop
    // that should be re-evaluated on each iteration.
    priv->condition = bytes_pub.create(condition, strlen(condition));
    if (!priv->condition)
    {
        BLAMMO(FATAL, "bytes_pub.create(%s) failed", condition);
        whileloop->destroy(whileloop);
        return NULL;
    }

    // List of raw (mostly) uninterpreted command lines consisting
    // of the body of the whileloop.  One exception to this is we'll
    // need to track the nested depth of an 'end' keyword (multi-use)
    priv->lines = chain_pub.create(bytes_pub.copy,
                                   bytes_pub.destroy);
    if (!priv->lines)
    {
        BLAMMO(FATAL, "chain_pub.create() failed");
        whileloop->destroy(whileloop);
        return NULL;
    }

    return whileloop;
}

//------------------------------------------------------------------------|
static void scallop_while_destroy(void * whileloop_ptr)
{
    scallop_while_t * whileloop = (scallop_while_t *) whileloop_ptr;
    if (!whileloop || !whileloop->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    scallop_while_priv_t * priv = (scallop_while_priv_t *) whileloop->priv;

    if (priv->lines)
    {
        priv->lines->destroy(priv->lines);
    }

    if (priv->condition)
    {
        priv->condition->destroy(priv->condition);
    }

    memzero(whileloop->priv, sizeof(scallop_while_priv_t));
    free(whileloop->priv);

    // zero out and destroy the public interface
    memzero(whileloop, sizeof(scallop_while_t));
    free(whileloop);
}

//------------------------------------------------------------------------|
static void scallop_while_append(scallop_while_t * whileloop, const char * line)
{
    scallop_while_priv_t * priv = (scallop_while_priv_t *) whileloop->priv;

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
static int scallop_while_runner(scallop_while_t * whileloop,
                                void * context)
{
    scallop_while_priv_t * priv = (scallop_while_priv_t *) whileloop->priv;
    scallop_t * scallop = (scallop_t *) context;
    int result = 0;

    // Need to perform substitution and evaluation on each iteration
    while (scallop->evaluate_condition(scallop,
                                       priv->condition->cstr(priv->condition),
                                       priv->condition->size(priv->condition)))
    {
        // Iterate through all lines and dispatch each
        result = scallop->run_lines(scallop, priv->lines);
    }

    return result;
}

//------------------------------------------------------------------------|
const scallop_while_t scallop_while_pub = {
    &scallop_while_create,
    &scallop_while_destroy,
    &scallop_while_append,
    &scallop_while_runner,
    NULL
};
