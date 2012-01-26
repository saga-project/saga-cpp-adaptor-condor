CONDOR ADAPTOR
==============

Overview
--------

The Condor adaptor for SAGA provides the ability for Condor and Condor-G users 
to access its services through the simple programmatic interface of SAGA.

Currently, the adaptor works in conjunction with the Condor command line tools
to communicate with the Condor scheduler and start jobs on the default universe.


Status
------

Running interactive jobs is currently not supported. Suspend/resume is also not
supported.

Setup
-----

The Condor adaptor requires local access to the Condor binaries. Currently
condor_submit, condor_rm and condor_q may be used as well. condor_status is
useful in testing to check the status of submitted jobs.

The path to the binaries must be supplied in the adaptor configuration file.
Assuming Condor is locally installed and working, all implemented functionality
should work.

The adaptor can be tested even without a local Condor installation, by providing
dummy scripts that forward the commands to an appropriate host, e.g., by ssh.
A possible (if minimal) such script is:

    #!/bin/sh
    ssh user@host "/bin/sh -lc '$0' $@"

With this approach all file staging will happen on the remote (ssh) host,
though.

Condor-G
--------

It is possible to use the adaptor with Condor-G resources

If you would work with Condor-G directly, you would add something like this 
into your Condor submit script in order to use a Condor-G resource:

    Universe        = grid
    grid_resource   = gt2 belhaven-1.renci.org:2119/jobmanager-condor

You can do the same thing through the SAGA Job API, simply by using the 
following URL in the job service constructor:

    saga::job::service js("condorg://belhaven-1.renci.org:2119/jobmanager-condor");


Security issues and mitigation strategy
---------------------------------------

When using the Condor command-line tools, we implicitly rely on the Condor setup
and on the security context of the user to provide appropriate security.

Furthermore, binaries are searched for and invoked only from user supplied paths
in the configuration option "saga.adaptors.<adaptor name>.cli.bin_path".

(TODO) Environment variables provided to Condor binaries should be minimized and
sanitized. Configuration options will be added to allow the user to specify
optional command-line arguments and environment variables to be used.

