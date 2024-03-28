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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

//------------------------------------------------------------------------|
// A scallop if-else conditional is close to a while loop construct, but
// executes only once, and also has two sets of lines rather than one.
// They are short-lived and exist only on the contruct stack, and are
// destroyed once run on stack pop.
typedef struct scallop_ifelse_t
{
    // If-else factory function
    struct scallop_ifelse_t * (*create)(const char * condition);

    // If-else destructor function
    void (*destroy)(void * ifelse);

    // Set which case 'append' should append to
    void (*which_lines)(struct scallop_ifelse_t * ifelse, bool which);

    // Append a line to the if-else conditional
    void (*append)(struct scallop_ifelse_t * ifelse, const char * line);

    // Run the if-else conditional
    int (*runner)(struct scallop_ifelse_t * ifelse, void * context);

    // Private data
    void * priv;
}
scallop_ifelse_t;

//------------------------------------------------------------------------|
extern const scallop_ifelse_t scallop_ifelse_pub;
