//------------------------------------------------------------------------|
// Copyright (c) 2018-2020 by Raymond M. Foulk IV (rfoulk@gmail.com)
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

//------------------------------------------------------------------------|
#include "main.h"
#include "blammo.h"
#include "console.h"
#include "command.h"
#include "scallop.h"
#include "builtin.h"

//------------------------------------------------------------------------|
typedef struct
{
    // Pointer to program name
    char * name;

    // Number of times a signal has been handled
    int sigcount;

    // Console for user I/O
    console_t * console;

    // Interactive CLI shell
    scallop_t * scallop;
}
app_data_t;

//------------------------------------------------------------------------|
// The module data container instance
static app_data_t app_data;
static app_data_t * app = &app_data;

//------------------------------------------------------------------------|
int main(int argc, char *argv[])
{
    int status = 0;

    init(argv[0]);
    parse(argc, argv);
    status = prompt();
    quit(status);

    // unreached
    return 0;
}

//------------------------------------------------------------------------|
void init(char * path)
{
    // Informational, warning, error, or fatal only, no debug or verbose
    // Also disable logging to stdout unless asked to later.
    BLAMMO_LEVEL(INFO);
    BLAMMO_STDOUT(false);

    signal(SIGINT, sighandler);

    // create things in memory that need to exist
    // even before parsing takes place, like if
    // runtime options need to be set on existing objects
    // e.g builtin registry of base components

    app->name = basename(path);

    // TODO: consider making inputf/outputf parameters.
    // Could redirect I/O over a tty for example.
    bytes_t * history_file = bytes_pub.print_create(".%s-history",
                                                    app->name);
    app->console = console_pub.create(stdin, stdout,
                                      history_file->cstr(history_file));
    history_file->destroy(history_file);

    // Create the interactive command interpreter scallop.
    // Inject the console as it will be needed for interaction.
    app->scallop = scallop_pub.create(app->console,
                                      register_builtin_commands,
                                      app->name);
}

//------------------------------------------------------------------------|
void sighandler(int signum)
{
    app->sigcount++;

    BLAMMO(INFO, "signum: %d  sigcount: %d", signum, app->sigcount);
    if (app->sigcount >= APP_MAX_SIGCOUNT )
    {
        BLAMMO(INFO, "quitting...");
        quit(signum);
    }
}

//------------------------------------------------------------------------|
void quit(int status)
{
    BLAMMO(INFO, "status: %d", status);

    // destroy all objects here
    if (app->scallop)
    {
        // Stop the interactive shell
        app->scallop->quit(app->scallop);
        app->scallop->destroy(app->scallop);
    }

    if (app->console)
    {
        app->console->destroy(app->console);
    }

    exit(status);
}

//------------------------------------------------------------------------|
int parse(int argc, char *argv[])
{
    BLAMMO(INFO, "");

    char line[APP_BUFFER_SIZE] = { 0 };
    const char * opts = "Vv:l:s:h";
    int option;
    extern char * optarg;

    // Process command line
    while ((option = getopt(argc, argv, opts)) > 0)
    {
        switch (option)
        {
            case 'V':
                // report version and quit
                app->console->print(app->console,
                        "%s version %s", app->name, APP_VERSION);
                quit(0);
                break;

            case 'v':
                // set log verbosity level
                BLAMMO_LEVEL(atoi(optarg));
                break;
                // TODO: Change BLAMMO to have a disable stdout option

            case 'l':
                // log to a filename
                BLAMMO_FILE(optarg);
                break;

            case 's':
                // load and run a script on startup
                snprintf(line, APP_BUFFER_SIZE, "source %s", optarg);
                app->scallop->dispatch(app->scallop, line);
                break;

            case 'h':
            default:
                usage(app->name, opts);
                quit(-1);
                break;
        }
    }

    // FIXME: ADJUST THIS TO MAKE IT WORK WITH EXTRA UNPARSED ARGS
    // Store excess arguments in scallop's variable
    // collection so dispatch can perform substitution.
//    app->scallop->store_args(app->scallop, argc, argv);

	return 0;
}

//------------------------------------------------------------------------|
void usage(const char * name, const char * opts)
{
    app->console->print(app->console,
        "usage: %s [%s]\r\n\r\n"
        "-V            Report version and quit\r\n"
        "\r\n"
        "-v <level>    Set log level threshold\r\n"
        "              0=VERBOSE 1=DEBUG 2=INFO 3=WARNING 4=ERROR 5=FATAL\r\n"
        "\r\n"
        "-l <path>     Set log file path (default is NULL)\r\n"
        "\r\n"
        "-s <path>     Source a script file immediately on startup\r\n"
        "\r\n"
        "-h            Show this help text and quit\r\n"
        "\r\n" , name, opts);
}

//------------------------------------------------------------------------|
int prompt()
{
    // enter interactive prompt
    return app->scallop->loop(app->scallop, true);
}
