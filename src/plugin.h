//------------------------------------------------------------------------|
// Copyright (c) 2024-202X by Raymond M. Foulk IV (rfoulk@gmail.com)
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

#include "scallop.h"

//------------------------------------------------------------------------|
// A plugin for Scallop.  This might represent either a static built-in
// set of commands, or else one that is loaded dynamically.

typedef struct scallop_plugin_t
{
    // Plugin factory function
    struct scallop_plugin_t * (*create)(const char * name,
                                        scallop_registration_f add,
                                        scallop_registration_f remove);

    // Plugin destructor function
    void (*destroy)(void * plugin);

    // Add the plugin to Scallop
    bool (*add)(struct scallop_plugin_t * plugin, scallop_t * scallop);

    // Remove the plugin from Scallop
    bool (*remove)(struct scallop_plugin_t * plugin, scallop_t * scallop);

    // Private data
    void * priv;
}
scallop_plugin_t;

//------------------------------------------------------------------------|
extern const scallop_plugin_t scallop_plugin_pub;
