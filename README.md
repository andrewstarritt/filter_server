# filter_server

filter_server provides the means to run any arbitary command, script or program,
that accepts input from standard input and writes its result to standard output
as a forking TCP/IP service.

    usage: filter_server [OPTIONS] port command args...
           filter_server [--help|-h]
           filter_server [--version|-v]

### Options:
--sessions, -s The maximum number of allowed simultaneous session or connections.
               This will be clamped to the range 1 to 80
               The default is 20 sessions.

--timeout, -t  The maximum time in seconds that a session is allowed to run for.
               It may be qualified with m, h, d or w for minutes, hours, days
               and weeks respectively. 'none' means no timeout.
               The timeout will be adjusted to be >= 1.0 seconds if needs be.
               The default is 1d.

--unzip, -u    Decompress the input (using gunzip) sent to the filter command.

--zip, -z      Compress output (using gzip) from the filter command.

--version, -v  Show program version and exit.

--help, -h     Show this help information and exit.


### Parameters:

port           The port number on whuich the service will run.
               Must be >= 1024 for non-root privileged users.

command        The command to be run. This must be on the PATH and/or specified
               using an absolute path.

args...        Optional arguments passed to the command executable.


### Example (trivial):

on server...

    filter_server -- 4242 stdbuf -oL tr 'a-z' 'A-Z'

stdbuf is an easy way to modify (output) buffering.

on client...

    ncat server_hosts 4242

Any text typed on the command line will be converted to upper case.

