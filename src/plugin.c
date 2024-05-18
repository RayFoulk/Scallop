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
#include "plugin.h"

//------------------------------------------------------------------------|
typedef struct
{
    // The short name of this plugin, e.g. "butter"
    bytes_t * name;

    // Whether this represents a dynamically loaded plugin, or static
    bool dynamic;

    // TODO: handle, function pointers for add/remove.

    // Function pointers for adding and removing this plugin
    scallop_registration_f add;
    scallop_registration_f remove;
}
scallop_plugin_priv_t;

//------------------------------------------------------------------------|
static scallop_plugin_t * scallop_plugin_create(const char * name,
                                                scallop_registration_f add,
                                                scallop_registration_f remove)
{
    OBJECT_ALLOC(scallop_, plugin);

    priv->name = bytes_pub.create(name, strlen(name));
    if (!priv->name)
    {
        BLAMMO(FATAL, "bytes_pub.create(%s) failed", name);
        plugin->destroy(plugin);
        return NULL;
    }


    // Caller-provided add function indicates the plugin is static
    priv->dynamic = (bool) !add;

    // TODO: If dynamic, construct file name and try to locate
    // recursively within working directory, or given a working
    // directory (as an option from scallop parent.
    // e.g. given "butter" find within $cwd "libbutter.so"
    // if not found then report error and return NULL.

    // GOOD PLACE TO START: Add ability to set working dir
    // from command line options (also command??) and track
    // and save that and pass it into scallop as a private.
    // ALSO: need a 'finder' utility -- options might include
    //  find dir versus file (type) and find-first versus
    //  find-all (return list).  The latter seems maybe more
    //  complicated than scope requires.

    return plugin;
}

//------------------------------------------------------------------------|
static void scallop_plugin_destroy(void * plugin_ptr)
{
    OBJECT_PTR(scallop_, plugin, plugin_ptr, );

    if (priv->name)
    {
        priv->name->destroy(priv->name);
    }

    OBJECT_FREE(scallop_, plugin);
}

//------------------------------------------------------------------------|
static bool scallop_plugin_add(scallop_plugin_t * plugin,
                               scallop_t * scallop)
{
    OBJECT_PRIV(scallop_, plugin);

    BLAMMO(WARNING, "NOT IMPLEMENTED");

    // TODO: Getting this far means either the file was found or else
    //  this is a static plugin.  If static, we must somehow know
    //  the function to be called to register all commands!!!
    //  Dynamical libraries whould be dlopened and dlsym here.
    //  try to find functions "plugin_add" and "plugin_remove",
    //  set those function pointers, and then call the add function.

    return true;
}

//------------------------------------------------------------------------|
static bool scallop_plugin_remove(scallop_plugin_t * plugin,
                                  scallop_t * scallop)
{
    OBJECT_PRIV(scallop_, plugin);

    BLAMMO(WARNING, "NOT IMPLEMENTED");

    // TODO: This hasn't been conceived of in the initial design, but
    //  the plugin should go through and unregister all commands that
    //  were registered during add() and also clean up all of it's
    //  allocated data, and dlclose()

    return true;
}

//------------------------------------------------------------------------|
const scallop_plugin_t scallop_plugin_pub = {
    &scallop_plugin_create,
    &scallop_plugin_destroy,
    &scallop_plugin_add,
    &scallop_plugin_remove,
    NULL
};
