==================
 The program tpm2
==================

The program tpm2 is the actual package manager that performs all day activities
like installing a package. It supports different operations where exactly one
must be specified as a commandline argument. Several more options maybe added to
control how the action is performed. In general command line attributes start
with two dashes ("--") like long options to gnu programs, and packages of files
specified don't.


Operations and other commandline options
========================================

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Argument
     - Description

   * - ``--version``
     - Print the program's version

   * - ``--install``
     - Install or upgrade the specified packages

   * - ``--reinstall``
     - Like install but reinstalls the specified packages even if the same
       version is already installed
       
   * - ``--policy``
     - Show the installed and available versions of name

   * - ``--show-version``
     - Print a package's version number of \`---' if it is not installed

   * - ``--remote``
     - Remove the specified packages and their config files if they were not
       modified
   
   * - ``--remove-unneeded``
     - Remove all packages that were marked as automatically installed and are
       not required by other packages that are marked as manually installed
       
   * - ``--list-installed``
     - List all installed packages

   * - ``--show-problems``
     - Show all problems with the current installation (i.e.  halfly installed
       packages after an interruption or missing dependencies)

   * - ``--recover``
     - Recover from a dirty state by deleting all packages that are in a dirty
       state (always possible due to atomic write operations to statues)

   * - ``--installation-graph``
     - Print the dependency graph in dot format; If packages are specified, they
       are added to the graph.

   * - ``--reverse-dependencies``
     - List the packages that depend on the specified package firectly or
       indirectly

   * - ``--mark-manual``
     - Mark the specified packages as manually installed

   * - ``--mark-auto``
     - Mark the specified packages as automatically installed

   * - ``--help``
     - Display this list of options
