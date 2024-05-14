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
#include "utils.h"              // memzero(), OBJECT macros
#include "blammo.h"
#include "console.h"
#include "chain.h"
#include "bytes.h"

// Scallop
#include "scallop.h"
#include "command.h"
#include "whilex.h"

//------------------------------------------------------------------------|
typedef struct
{
    // The raw unevaluated conditional expression associated with this
    bytes_t * condition;

    // Raw command lines consisting of the while body
    chain_t * lines;
}
scallop_whilex_priv_t;

//------------------------------------------------------------------------|
static scallop_whilex_t * scallop_whilex_create(const char * condition)
{
    scallop_whilex_t * whilex = (scallop_whilex_t *) malloc(
            sizeof(scallop_whilex_t));
    if (!whilex)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_whilex_t)) failed");
        return NULL;
    }

    memcpy(whilex, &scallop_whilex_pub, sizeof(scallop_whilex_t));

    whilex->priv = malloc(sizeof(scallop_whilex_priv_t));
    if (!whilex->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(scallop_whilex_priv_t)) failed");
        free(whilex);
        return NULL;
    }

    memzero(whilex->priv, sizeof(scallop_whilex_priv_t));
    scallop_whilex_priv_t * priv = (scallop_whilex_priv_t *) whilex->priv;

    // The conditional expression associated with the while loop
    // that should be re-evaluated on each iteration.
    priv->condition = bytes_pub.create(condition, strlen(condition));
    if (!priv->condition)
    {
        BLAMMO(FATAL, "bytes_pub.create(%s) failed", condition);
        whilex->destroy(whilex);
        return NULL;
    }

    // List of raw (mostly) uninterpreted command lines consisting
    // of the body of the whilex.  One exception to this is we'll
    // need to track the nested depth of an 'end' keyword (multi-use)
    priv->lines = chain_pub.create(bytes_pub.copy,
                                   bytes_pub.destroy);
    if (!priv->lines)
    {
        BLAMMO(FATAL, "chain_pub.create() failed");
        whilex->destroy(whilex);
        return NULL;
    }

    return whilex;
}

//------------------------------------------------------------------------|
static void scallop_whilex_destroy(void * whilex_ptr)
{
    scallop_whilex_t * whilex = (scallop_whilex_t *) whilex_ptr;
    if (!whilex || !whilex->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    scallop_whilex_priv_t * priv = (scallop_whilex_priv_t *) whilex->priv;

    if (priv->lines)
    {
        priv->lines->destroy(priv->lines);
    }

    if (priv->condition)
    {
        priv->condition->destroy(priv->condition);
    }

    memzero(whilex->priv, sizeof(scallop_whilex_priv_t));
    free(whilex->priv);

    // zero out and destroy the public interface
    memzero(whilex, sizeof(scallop_whilex_t));
    free(whilex);
}

//------------------------------------------------------------------------|
static void scallop_whilex_append(scallop_whilex_t * whilex, const char * line)
{
    scallop_whilex_priv_t * priv = (scallop_whilex_priv_t *) whilex->priv;

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
static int scallop_whilex_runner(scallop_whilex_t * whilex,
                                void * context)
{
    scallop_whilex_priv_t * priv = (scallop_whilex_priv_t *) whilex->priv;
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
const scallop_whilex_t scallop_whilex_pub = {
    &scallop_whilex_create,
    &scallop_whilex_destroy,
    &scallop_whilex_append,
    &scallop_whilex_runner,
    NULL
};
