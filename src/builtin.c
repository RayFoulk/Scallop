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
#include <errno.h>

// Scallop
#include "scallop.h"
#include "command.h"
#include "routine.h"
#include "whilex.h"
#include "ifelse.h"
#include "parser.h"
#include "builtin.h"

// RayCO
#include "console.h"
#include "chain.h"
#include "bytes.h"
#include "utils.h"
#include "blammo.h"

//------------------------------------------------------------------------|
// Built-In command handler functions
static int builtin_handler_help(void * scmd,
                                void * context,
                                int argc,
                                char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);
    scallop_cmd_t * cmds = scallop->commands(scallop);

    // If the caller provided additional keywords I.E. "help thingy"
    // then try to provide help for the specified command instead
    // of just everything (by default)
    scallop_cmd_t * focus = NULL;
    scallop_cmd_t * found = NULL;

    // TODO: Support more than one topic
    if (argc > 1)
    {
        found = cmds->find_by_keyword(cmds, args[1]);
        if (!found)
        {
            console->error(console, "command %s not found", args[1]);
            return ERROR_MARKER_DEC;
        }

        // Make a copy of found command so the original doesn't get destroyed
        found = found->copy(found);

        focus = scallop_cmd_pub.create(NULL, NULL, NULL, NULL, NULL);
        focus->register_cmd(focus, found);
        cmds = focus;
    }

    // Getting the longest command has to occur before diving into
    // recursive help, because the top level help must be told.
    // otherwise, recursive help will only determine the longest
    // for each sub-branch.
    const char * start = "\r\ncommands:\r\n\r\n";
    bytes_t * help = bytes_pub.create(start, strlen(start));
    size_t longest_kw_and_hints = 0;
    cmds->longest(cmds, &longest_kw_and_hints, NULL, NULL, NULL);

    // TODO: Improve help column formatting so that indented sub-commands
    //  align consistently with base level help.
    int result = cmds->help(cmds, help, 0, longest_kw_and_hints);
    if (result < 0)
    {
        console->error(console, "help for commands failed with %d", result);
        help->destroy(help);
        return result;
    }

    console->print(console, "%s", help->cstr(help));
    help->destroy(help);
    if (focus) { focus->destroy(focus); }

    return result;
}

//------------------------------------------------------------------------|
static int builtin_handler_alias(void * scmd,
                                 void * context,
                                 int argc,
                                 char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    // TODO: Consider supporting aliasing nested commands
    //  via some syntax like 'alias stuff log.file'.  or else
    //  change root command context 'change log' and then
    // 'alias stuff file'.  Then again, maybe we're going over
    // the rails a bit here.

    // want to actually register a new command here, under
    // the keyword of the alias, but with the callback of the
    // original keyword.  TBD under what scope this applies.
    // NOTE: this is distinct from bash's alias which can
    // encompass pretty much anything.
    // this will be saved for the 'routine' keyword
    if (argc < 2)
    {
        console->error(console, "expected an alias keyword");
        return ERROR_MARKER_DEC;
    }
    else if (argc < 3)
    {
        console->error(console, "expected a command to be aliased");
        return ERROR_MARKER_DEC;
    }

    // Find the command that is referenced
    scallop_cmd_t * scope = scallop->commands(scallop);
    scallop_cmd_t * cmd = scope->find_by_keyword(scope, args[2]);
    if (!cmd)
    {
        console->error(console, "command %s not found", args[2]);
        return ERROR_MARKER_DEC;
    }

    // re-register the same command under a different keyword
    scallop_cmd_t * alias = scope->alias(cmd, args[1]);
    if (!scope->register_cmd(scope, alias))
    {
        console->error(console, "failed to register alias %s to %s",
                                args[1], args[2]);
        return ERROR_MARKER_DEC;
    }

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_unregister(void * scmd,
                                      void * context,
                                      int argc,
                                      char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a command keyword to unregister");
        return ERROR_MARKER_DEC;
    }

    scallop_cmd_t * scope = scallop->commands(scallop);
    scallop_cmd_t * cmd = scope->find_by_keyword(scope, args[1]);
    if (!cmd)
    {
        console->error(console, "command %s not found", args[1]);
        return ERROR_MARKER_DEC;
    }

    if (!cmd->is_mutable(cmd))
    {
        console->error(console,
                       "can't unregister immutable command \'%s\'",
                       cmd->keyword(cmd));
        return ERROR_MARKER_DEC;
    }

    // NOTE: It is highly unlikely that a command would be both
    // mutable AND a language construct.  Even if so, the construct
    // stack should probably be left alone here.

    // remove the associated routine if the command was also a routine.
    // This will just fail without consequence if the command is not.
    scallop->routine_remove(scallop, cmd->keyword(cmd));
    // FIXME: When routines are moved out into butter plugin,
    //  There will need to be a callback or notification made.
    //  something like scallop->plugins->notify_unreg()

    // Unregister the command -- this also destroys the command
    if (!scope->unregister_cmd(scope, cmd))
    {
        console->error(console,
                       "unregister_cmd(%s) failed",
                       cmd->keyword(cmd));
        return ERROR_MARKER_DEC;
    }

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_log(void * scmd,
                               void * context,
                               int argc,
                               char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a log sub-command");
        return ERROR_MARKER_DEC;
    }

    // Find and execute subcommand
    scallop_cmd_t * log = (scallop_cmd_t *) scmd;
    scallop_cmd_t * cmd = log->find_by_keyword(log, args[1]);
    if (!cmd)
    {
        console->error(console, "log sub-command %s not found", args[1]);
        return ERROR_MARKER_DEC;
    }

    // TODO: The answer to nested help may be staring me in the face here,
    // versus a complex approach to having parent links in all commands.
    // either-or, but the to-do is to evaluate approaches here and make
    // a decision.
    return cmd->exec(cmd, --argc, &args[1]);
}

//------------------------------------------------------------------------|
static int builtin_handler_log_level(void * scmd,
                                     void * context,
                                     int argc,
                                     char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a numeric log level 0-5");
        return ERROR_MARKER_DEC;
    }

    BLAMMO_DECLARE(size_t level = strtoul(args[1], NULL, 0));
    BLAMMO(INFO, "Setting log level to %u", level);
    BLAMMO_LEVEL(level);

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_log_stdout(void * scmd,
                                      void * context,
                                      int argc,
                                      char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a boolean value");
        return ERROR_MARKER_DEC;
    }

    BLAMMO(INFO, "Setting log stdout to %s",
                 str_to_bool(args[1]) ? "true" : "false");
    BLAMMO_STDOUT(str_to_bool(args[1]));

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_log_file(void * scmd,
                                    void * context,
                                    int argc,
                                    char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a file path/name");
        return ERROR_MARKER_DEC;
    }

    BLAMMO(INFO, "Setting log file path to %s", args[1]);
    BLAMMO_FILE(args[1]);

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_plugin(void * scmd,
                                  void * context,
                                  int argc,
                                  char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a plugin sub-command");
        return ERROR_MARKER_DEC;
    }

    // Find and execute subcommand
    scallop_cmd_t * plugin = (scallop_cmd_t *) scmd;
    scallop_cmd_t * cmd = plugin->find_by_keyword(plugin, args[1]);
    if (!cmd)
    {
        console->error(console, "plugin sub-command %s not found", args[1]);
        return ERROR_MARKER_DEC;
    }

    return cmd->exec(cmd, --argc, &args[1]);
}

//------------------------------------------------------------------------|
static int builtin_handler_plugin_add(void * scmd,
                                      void * context,
                                      int argc,
                                      char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a plugin name to add");
        return ERROR_MARKER_DEC;
    }

    // TODO: DO STUFF

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_plugin_remove(void * scmd,
                                         void * context,
                                         int argc,
                                         char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a plugin name to remove");
        return ERROR_MARKER_DEC;
    }

    // TODO: DO STUFF

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_plugin_list(void * scmd,
                                       void * context,
                                       int argc,
                                       char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    // TODO: DO STUFF

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_print(void * scmd,
                                 void * context,
                                 int argc,
                                 char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);
    long result = 0;
    int argnum = 1;

    // Need something to print!
    if (argc < 2)
    {
        console->error(console, "expected an expression to print");
        return ERROR_MARKER_DEC;
    }

    // Print each argument/expression on its own line
    // only keep track of and return the most recent result
    for (argnum = 1; argnum < argc; argnum++)
    {
        // Still testing this approach of having the sparser
        // determine if the expression should be evaluated.
        // 'print' tries to mimic the behavior of 'assign'
        // Only printing instead of assigning.
        // TODO: Export this from scallop public interface
        //  similar to evaluate_condition() used by while & if-else
        //  so that registered handlers are not directly accessing
        //  supposed-to-be-private parts of scallop
        // TODO: Also allow variables to be object references and
        //  introduce a print-function registry per variable.
        //  class: variable, with type, and pretty-print callback.
        if (sparser_is_expr(args[argnum]))
        {
            result = sparser_evaluate(console->error, console, args[argnum]);
            if (result == SPARSER_INVALID_EXPRESSION)
            {
                console->error(console,
                               "invalid expression \'%\'",
                               args[argnum]);
            }
            else
            {
                console->print(console, "%ld", result);
            }
        }
        else
        {
            console->print(console, "%s", args[argnum]);
        }
    }

    return (int) result;
}

//------------------------------------------------------------------------|
static int builtin_handler_assign(void * scmd,
                                  void * context,
                                  int argc,
                                  char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);
    long result = 0;

    if (argc < 2)
    {
        console->error(console, "expected a variable name");
        return ERROR_MARKER_DEC;
    }
    else if (argc < 3)
    {
        console->error(console, "expected a variable value");
        return ERROR_MARKER_DEC;
    }

    // Evaluate expression down to a numeric value if it looks like
    // we should, based on some very shallow and flakey criteria
    if (sparser_is_expr(args[2]))
    {
        result = sparser_evaluate(console->error, console, args[2]);
        if (result == SPARSER_INVALID_EXPRESSION)
        {
            console->error(console,
                           "not assigning \'%s\' from invalid expression \'%\'",
                           args[1],
                           args[2]);
            return ERROR_MARKER_DEC;
        }

        // Numeric assignment
        bytes_t * value = bytes_pub.print_create("%ld", result);
        scallop->assign_variable(scallop, args[1], value->cstr(value));
        value->destroy(value);
    }
    else
    {
        // Direct string assignment
        scallop->assign_variable(scallop, args[1], args[2]);
    }

    return result;
}

//------------------------------------------------------------------------|
static int builtin_handler_source(void * scmd,
                                  void * context,
                                  int argc,
                                  char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a file path argument");
        return ERROR_MARKER_DEC;
    }

    FILE * source = fopen(args[1], "r");
    if (!source)
    {
        console->error(console, "could not open %s for reading", args[1]);
        return ERROR_MARKER_DEC;
    }

    // Store script arguments in scallop's variable
    // collection so dispatch can perform substitution.
    scallop->store_args(scallop, argc, args);

    // Stash the current console input source & swap to source file
    FILE * input = console->get_inputf(console);
    console->set_inputf(console, source);

    // Now get an dispatch commands from the script until EOF
    int result = scallop->run_console(scallop, false);

    // Put console back to original state
    console->set_inputf(console, input);

    // Done with script file
    fclose(source);
    return result;
}

//------------------------------------------------------------------------|
static int builtin_linefunc_routine(void * context,
                                    void * object,
                                    const char * line)
{
    scallop_t * scallop = (scallop_t *) context;
    scallop_rtn_t * routine = (scallop_rtn_t *) object;

    if (!routine)
    {
        BLAMMO(VERBOSE, "dry run routine linefunc");
        return 0;
    }

    // put the raw line as-is in the routine.  variable substitutions
    // and tokenization will occur later during routine execution.
    routine->append(routine, line);
    (void) scallop;

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_popfunc_routine(void * context,
                                   void * object)
{
    scallop_t * scallop = (scallop_t *) context;
    scallop_rtn_t * routine = (scallop_rtn_t *) object;
    scallop_cmd_t * cmds = scallop->commands(scallop);

    if (!routine)
    {
        BLAMMO(VERBOSE, "dry run routine popfunc");
        return 0;
    }


    bool success = true;

    // All done defining this routine.  Now register it as a
    // proper command.
    scallop_cmd_t * cmd = cmds->create(
            routine->handler,
            scallop,
            routine->name(routine),
            " [argument-list]",
            "user-registered routine");

    // should be allowed to delete/modify routines
    cmd->set_attributes(cmd, SCALLOP_CMD_ATTR_MUTABLE);

    success = cmds->register_cmd(cmds, cmd);

    return success ? 0 : -1;
}

//------------------------------------------------------------------------|
// This routine performs the _definition_ of a routine.  Future incoming
// lines will be handled by the 'routine' language construct's linefunc
// callback.  Lines may come from the user interactively or from a script.
// The actual _execution_ of a routine will occur later after the popfunc
// is called and the routine is registered.
static int builtin_handler_routine(void * scmd,
                                   void * context,
                                   int argc,
                                   char ** args)
{
    scallop_cmd_t * cmd = (scallop_cmd_t *) scmd;
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a routine name");
        return ERROR_MARKER_DEC;
    }

    scallop_rtn_t * routine = NULL;
    const char * routine_name = NULL;

    // For dry run, do not create a routine object
    if (cmd->is_dry_run(cmd))
    {
        cmd->clear_attributes(cmd, SCALLOP_CMD_ATTR_DRY_RUN);
    }
    else
    {
        // Check if there is already a routine by the given name
        routine = scallop->routine_by_name(scallop, args[1]);
        if (routine)
        {
            console->error(console,
                           "routine \'%s\' already exists",
                           args[1]);
            return ERROR_MARKER_DEC;
        }

        // Create a unique new routine object
        routine = scallop->routine_insert(scallop, args[1]);
        if (!routine)
        {
            console->error(console, "create routine \'%s\' failed", args[1]);
            return ERROR_MARKER_DEC;
        }

        routine_name = routine->name(routine);
    }

    // Push the new routine definition onto the language construct
    // stack.  This should get popped off when the matching 'end'
    // statement is reached, and at THAT point it will become
    // registered as a new command (with the ubiquitous routine handler)
    // Until then, incoming lines will be added to the construct.
    scallop->construct_push(scallop,
                            routine_name,
                            context,
                            routine,
                            builtin_linefunc_routine,
                            builtin_popfunc_routine);

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_linefunc_while(void * context,
                                  void * object,
                                  const char * line)
{
    BLAMMO(VERBOSE, "");

    scallop_t * scallop = (scallop_t *) context;
    scallop_whilex_t * whilex = (scallop_whilex_t *) object;

    if (!whilex)
    {
        BLAMMO(VERBOSE, "dry run while loop linefunc");
        return 0;
    }

    // put the raw line as-is in the routine.  variable substitutions
    // and tokenization will occur later during while execution.
    whilex->append(whilex, line);
    (void) scallop;

    return 0;
}

//------------------------------------------------------------------------|
// One approach considered was to create a special unregistered and
// ephemeral command type, containing an object pointer, but this
// approach is probably overly complicated when we have the object
// right here as called from construct_pop.
static int builtin_popfunc_while(void * context,
                                 void * object)
{
    scallop_whilex_t * whilex = (scallop_whilex_t *) object;

    // NOTE: During definition of a routine containing a while loop,
    // this will have no lines to execute and the condition will exist
    // but cannot be evaluated due to substitution not having occurred.
    if (!whilex)
    {
        BLAMMO(VERBOSE, "dry run while loop popfunc");
        return 0;
    }

    // For normal while loop in base context, the loop should NOW be
    // executed (call the whilex->handler). Then, when it is finished,
    // destroy it.  While loops are like immediate ephemeral functions,
    // that don't take arguments.
    int result = whilex->runner(whilex, context);

    // While loop evaporates
    whilex->destroy(whilex);

    return result;
}

//------------------------------------------------------------------------|
// while loops are shorter-lived constructs than routines.  They only
// live inside the construct stack and evaporate when out of scope.
static int builtin_handler_while(void * scmd,
                                 void * context,
                                 int argc,
                                 char ** args)
{
    BLAMMO(VERBOSE, "");

    scallop_cmd_t * cmd = (scallop_cmd_t *) scmd;
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a conditional expression");
        return ERROR_MARKER_DEC;
    }

    // Create a while loop construct
    scallop_whilex_t * whilex = NULL;

    // For dry run, do not create a while loop object
    if (cmd->is_dry_run(cmd))
    {
        cmd->clear_attributes(cmd, SCALLOP_CMD_ATTR_DRY_RUN);
    }
    else
    {
        whilex = scallop_whilex_pub.create(args[1]);
        if (!whilex)
        {
            console->error(console, "create while \'%s\' failed", args[1]);
            return ERROR_MARKER_DEC;
        }
    }

    // Push the new while loop onto the language construct
    // stack.  This should get popped off when the matching 'end'
    // statement is reached, and at THAT point it should execute ONLY IF
    // in the base context, and NOT while in the middle of defining a
    // routine. Until then, incoming lines will be added to the construct.
    scallop->construct_push(scallop,
        "while",                // TODO: Consider names for while???
        context,
        whilex,
        builtin_linefunc_while,
        builtin_popfunc_while);

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_linefunc_if(void * context,
                               void * object,
                               const char * line)
{
    BLAMMO(VERBOSE, "");

    scallop_t * scallop = (scallop_t *) context;
    scallop_ifelse_t * ifelse = (scallop_ifelse_t *) object;

    if (!ifelse)
    {
        BLAMMO(VERBOSE, "dry run if-else linefunc");
        return 0;
    }

    ifelse->append(ifelse, line);
    (void) scallop;

    return 0;
}

//------------------------------------------------------------------------|
static int builtin_popfunc_if(void * context,
                              void * object)
{
    scallop_ifelse_t * ifelse = (scallop_ifelse_t *) object;

    if (!ifelse)
    {
        BLAMMO(VERBOSE, "dry run if-else linefunc");
        return 0;
    }

    int result = ifelse->runner(ifelse, context);

    // While loop evaporates
    ifelse->destroy(ifelse);

    return result;
}


//------------------------------------------------------------------------|
// if-else statements are short-lived like while loops.
static int builtin_handler_if(void * scmd,
                              void * context,
                              int argc,
                              char ** args)
{
    BLAMMO(VERBOSE, "");

    scallop_cmd_t * cmd = (scallop_cmd_t *) scmd;
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (argc < 2)
    {
        console->error(console, "expected a conditional expression");
        return ERROR_MARKER_DEC;
    }

    // Create an if-else construct
    scallop_ifelse_t * ifelse = NULL;

    // For dry run, do not create a while loop object
    if (cmd->is_dry_run(cmd))
    {
        cmd->clear_attributes(cmd, SCALLOP_CMD_ATTR_DRY_RUN);
    }
    else
    {
        ifelse = scallop_ifelse_pub.create(args[1]);
        if (!ifelse)
        {
            console->error(console, "create ifelse \'%s\' failed", args[1]);
            return ERROR_MARKER_DEC;
        }
    }

    scallop->construct_push(scallop,
        "if-else",                // TODO: Consider names for ifelse???
        context,
        ifelse,
        builtin_linefunc_if,
        builtin_popfunc_if);

    return 0;
}


//------------------------------------------------------------------------|
// 'else' is a special command keyword, in that it modifies the existing
// construct declaration (assuming it's an if-else!)
static int builtin_handler_else(void * scmd,
                                void * context,
                                int argc,
                                char ** args)
{
    BLAMMO(VERBOSE, "");

    scallop_cmd_t * cmd = (scallop_cmd_t *) scmd;
    scallop_t * scallop = (scallop_t *) context;
    console_t * console = scallop->console(scallop);

    if (cmd->is_dry_run(cmd))
    {
        cmd->clear_attributes(cmd, SCALLOP_CMD_ATTR_DRY_RUN);
        return 0;
    }

    scallop_ifelse_t * ifelse = (scallop_ifelse_t *)
            scallop->construct_object(scallop);
    if (!ifelse)
    {
        console->error(console, "else without if construct");
        return ERROR_MARKER_DEC;
    }

    // Change the set of lines that are appended
    ifelse->which_lines(ifelse, false);
    return 0;
}

//------------------------------------------------------------------------|
static int builtin_handler_end(void * scmd,
                               void * context,
                               int argc,
                               char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    return scallop->construct_pop(scallop);
}

//------------------------------------------------------------------------|
static int builtin_handler_quit(void * scmd,
                                void * context,
                                int argc,
                                char ** args)
{
    scallop_t * scallop = (scallop_t *) context;
    scallop->quit(scallop);
    return 0;
}

//------------------------------------------------------------------------|
bool register_builtin_commands(void * scallop_ptr)
{
    scallop_t * scallop = (scallop_t *) scallop_ptr;
    scallop_cmd_t * cmds = scallop->commands(scallop);
    scallop_cmd_t * cmd = NULL;
    bool success = true;

    // TODO: CORE -- IMPORT / MODULE / LOAD
    // plugin load butter
    // plugin unload butter
    // plugin list

    // CORE
    // TODO? To assist with allowing 'help' to target specific subcommands,
    // all base level commands would have to be re-registered
    // as subcommands under help in order for tab-completion to work.
    // Is this even feasible?  Or would it break the whole model?
    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_help,
        scallop,
        "help",
        NULL,
        "show a list of commands with hints and description"));

    // CORE
    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_quit,
        scallop,
        "quit",
        NULL,
        "exit the scallop command handling loop"));

    // CORE
    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_alias,
        scallop,
        "alias",
        " <alias-keyword> <original-keyword>",
        "alias one command keyword to another"));

    // TODO: Also, removing a thing should ALWAYS remove all
    //  of it's aliases.  otherwise the aliases are present
    //  but invalid and could cause weird crashes/undefined behavior.
    // CORE -- TODO: Also use this to clear variables/objects
    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_unregister,
        scallop,
        "unreg",
        " <command-keyword>",
        "unregister a mutable command"));

    // CORE
    scallop_cmd_t * log = cmds->create(
        builtin_handler_log,
        scallop,
        "log",
        " <log-command> <...>",
        "change blammo logger options");

    success &= cmds->register_cmd(cmds, log);

    // CORE
    success &= log->register_cmd(log, log->create(
        builtin_handler_log_level,
        scallop,
        "level",
        " <0..5>",
        "change the blammo log message level (0=VERBOSE, 5=FATAL)"));

    // CORE
    success &= log->register_cmd(log, log->create(
        builtin_handler_log_stdout,
        scallop,
        "stdout",
        " <true/false>",
        "enable or disable logging to stdout"));

    // CORE
    success &= log->register_cmd(log, log->create(
        builtin_handler_log_file,
        scallop,
        "file",
        " <log-file-path>",
        "change the blammo log file path"));


    // CORE
    scallop_cmd_t * plugin = cmds->create(
        builtin_handler_plugin,
        scallop,
        "plugin",
        " <plugin-command> <...>",
        "add, remove, or list plugins");

    success &= cmds->register_cmd(cmds, plugin);

    // CORE
    success &= plugin->register_cmd(plugin, plugin->create(
        builtin_handler_plugin_add,
        scallop,
        "add",
        " <plugin-name>",
        "add a plugin to scallop"));

    // CORE
    success &= plugin->register_cmd(plugin, plugin->create(
        builtin_handler_plugin_remove,
        scallop,
        "remove",
        " <plugin-name>",
        "remove a plugin from scallop"));

    // CORE
    success &= plugin->register_cmd(plugin, plugin->create(
        builtin_handler_plugin_list,
        scallop,
        "list",
        "",
        "list all currently loaded plugins"));


    // BASE LANGUAGE?? -- refactor necessary??
    // POSSIBLY CORE - print function registry associated
    // with objects in place of variables - type
    // could be either 'basic/string' or 'object'
    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_print,
        scallop,
        "print",
        " [arbitrary-expression(s)]",
        "print expressions, strings, and variables"));

    // BASE LANGUAGE - MARGINAL
    // Will need to be CORE in order to run general script
    // otherwise a module load from command line would
    // always be needed.
    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_source,
        scallop,
        "source",
        " <script-path>",
        "load and run a command script"));

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    // TODO: Move these into butter plugin

    // BASE LANGUAGE - MARGINAL
    success &= cmds->register_cmd(cmds, cmds->create(
        builtin_handler_assign,
        scallop,
        "assign",
        " <var-name> <value>",
        "assign a value to a variable"));


    // BASE LANGUAGE
    cmd = cmds->create(
        builtin_handler_routine,
        scallop,
        "routine",
        " <routine-name> ...",
        "define and register a new routine");
    cmd->set_attributes(cmd, SCALLOP_CMD_ATTR_CONSTRUCT_PUSH);
    success &= cmds->register_cmd(cmds, cmd);

    // BASE LANGUAGE
    cmd = cmds->create(
        builtin_handler_while,
        scallop,
        "while",
        " (expression)",
        "declare a while-loop construct");
    cmd->set_attributes(cmd, SCALLOP_CMD_ATTR_CONSTRUCT_PUSH);
    success &= cmds->register_cmd(cmds, cmd);

    // BASE LANGUAGE
    cmd = cmds->create(
        builtin_handler_if,
        scallop,
        "if",
        " (expression)",
        "declare an if-else construct. else is optional");
    cmd->set_attributes(cmd, SCALLOP_CMD_ATTR_CONSTRUCT_PUSH);
    success &= cmds->register_cmd(cmds, cmd);

    // BASE LANGUAGE
    cmd = cmds->create(
        builtin_handler_else,
        scallop,
        "else",
        "",
        "denotes the \'else\' part of an if-else construct");
    cmd->set_attributes(cmd, SCALLOP_CMD_ATTR_CONSTRUCT_MODIFIER);
    success &= cmds->register_cmd(cmds, cmd);

    // BASE LANGUAGE
    cmd = cmds->create(
        builtin_handler_end,
        scallop,
        "end",
        NULL,
        "finalize a multi-line language construct");
    cmd->set_attributes(cmd, SCALLOP_CMD_ATTR_CONSTRUCT_POP);
    success &= cmds->register_cmd(cmds, cmd);

    return success;
}
