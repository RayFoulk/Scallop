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
// A scallop while loop is very similar to a routine, but it is not named
// and executes at a different point in the context.  Rather than being
// called, it executes it's stored lines when the construct stack is in
// a certain state TBD.  It will also continue to execute repeatedly while
// the conditional expression associated with the while loop is true.

typedef struct scallop_while_t
{
    // While factory function
    struct scallop_while_t * (*create)(const char * condition);

    // While destructor function
    void (*destroy)(void * whileloop);

    // Append a line to the while loop
    void (*append)(struct scallop_while_t * whileloop, const char * line);

    // Run the while loop
    int (*runner)(struct scallop_while_t * whileloop, void * context);

    // Private data
    void * priv;
}
scallop_while_t;

//------------------------------------------------------------------------|
extern const scallop_while_t scallop_while_pub;
