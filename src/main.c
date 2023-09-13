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
    char * progname;

    // Number of times a signal has been handled
    int sigcount;

    // Console for user I/O
    console_t * console;

    // Interactive CLI shell
    scallop_t * scallop;
}
raytl_data_t;

//------------------------------------------------------------------------|
// The module data container instance
static raytl_data_t raytl_data;
static raytl_data_t * raytl = &raytl_data;

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

    raytl->progname = basename(path);

    // TODO: consider making inputf/outputf parameters.
    // Could redirect I/O over a tty for example.
    bytes_t * history_file = bytes_pub.print_create(".%s-history",
                                                    raytl->progname);
    raytl->console = console_pub.create(stdin, stdout,
                                        history_file->cstr(history_file));
    history_file->destroy(history_file);

    // Create the interactive command interpreter scallop.
    // Inject the console as it will be needed for interaction.
    raytl->scallop = scallop_pub.create(raytl->console,
                                        register_builtin_commands,
                                        raytl->progname);

}

//------------------------------------------------------------------------|
void sighandler(int signum)
{
    raytl->sigcount++;

    BLAMMO(INFO, "signum: %d  sigcount: %d", signum, raytl->sigcount);
    if (raytl->sigcount >= RAYTL_MAX_SIGCOUNT )
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
    if (raytl->scallop)
    {
        // Stop the interactive shell
        raytl->scallop->quit(raytl->scallop);
        raytl->scallop->destroy(raytl->scallop);
    }

    if (raytl->console)
    {
        raytl->console->destroy(raytl->console);
    }

    exit(status);
}

//------------------------------------------------------------------------|
int parse(int argc, char *argv[])
{
    BLAMMO(INFO, "");

    char line[RAYTL_BUFFER_SIZE] = { 0 };
    const char * opts = "Vv:l:s:n:h";
    int option;
    extern char * optarg;

    // Process command line
    while ((option = getopt (argc, argv, opts)) > 0)
    {
        switch (option)
        {
            case 'V':
                // report version and quit
                raytl->console->print(raytl->console,
                        "%s version %s", raytl->progname, RAYTL_VERSION);
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
                snprintf(line, RAYTL_BUFFER_SIZE, "source %s", optarg);
                raytl->scallop->dispatch(raytl->scallop, line);
                break;

            case 'n':
                // notify events on a UNIX socket, or TCP
                break;

            case 'h':
            default:
                usage(raytl->progname, opts);
                quit(-1);
                break;
        }
    }

	return 0;
}

//------------------------------------------------------------------------|
void usage(const char * progname, const char * opts)
{
    raytl->console->print(raytl->console,
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
        "\r\n" , progname, opts);
}

//------------------------------------------------------------------------|
int prompt()
{
    // enter interactive prompt
    return raytl->scallop->loop(raytl->scallop, true);
}
