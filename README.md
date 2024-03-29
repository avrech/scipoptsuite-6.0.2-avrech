# Important Notes
This scipoptsuite version implements the backend for my other project at
`https://github.com/avrech/learning2cut.git`.  

## Installation
- add `export SCIPOPTDIR=/path/to/installation/dir` to your `.bashrc`  
- go to `scipoptsuite-6.0.2-avrech` and run the following commands in the terminal
> $ cmake -Bbuild -H. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$SCIPOPTDIR  
> $ cmake --build build  
> $ cd build  
> $ make install  

## Added Functionality 
The following files in `scip/src/scip/` were modified to enable cut selection control 
via PySCIPOpt:
- scip.h, pub_lp.h, struct_sepastore.h, sepastore.h 
- solve.c, sepastore.c, lp.c  

The following functionality was added:  
- get the recently selected cut names
- force selecting a specified group of cuts  
- reset `maxcuts` and `maxcutsroot` at the beginning of each separation round, to avoid undesired `enoughcuts` case. 

In order to use this functionality, please clone PySCIPOpt from `https://github.com/ds4dm/PySCIPOpt.git` and 
install the `ml-cutting-planes` branch inside your virtualenv.

```
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*                                                                           *
*                  This file is part of the program and library             *
*         SCIP --- Solving Constraint Integer Programs                      *
*                                                                           *
*    Copyright (C) 2002-2019 Konrad-Zuse-Zentrum                            *
*                            fuer Informationstechnik Berlin                *
*                                                                           *
*  SCIP is distributed under the terms of the ZIB Academic License.         *
*                                                                           *
*  You should have received a copy of the ZIB Academic License              *
*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      *
*                                                                           *
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
```

The SCIP Optimization Suite consists of the following five software tools:
  1. ZIMPL  - the Zuse Institute Mathematical Programming language
  2. SoPlex - the Sequential Object Oriented Simplex
  3. SCIP   - Solving Constraint Integer Programs
  4. GCG    - Generic Column Generation
  5. UG     - Ubiquity Generator Framework

We provide two different systems to compile the code: the traditional Makefile
system and the new CMake build system.  Be aware that generated libraries and
binaries of both systems might be different and incompatible.  For further
details please refer to the INSTALL file of SCIP and the online documentation.


Content
=======

1. CMake

2. Makefiles
  - Creating a SCIP binary and the individual libraries
  - Creating a single library containing SCIP, SoPlex, and ZIMPL
  - Creating GCG and UG


CMake
=====

Ensure that you're using an up-to-date [CMake installation](https://cmake.org/).
CMake will automatically configure the code according to your environment and
the available third-party tools, like GMP, etc.
Create a new directory for the build, for instance inside the `scipoptsuite` directory.

    mkdir build
    cd build
    cmake ..
    make

For further information please refer to the online documentation of [SCIP](http://scip.zib.de)
or the `INSTALL` in the SCIP subdirectory.


Makefiles
=========

##  Creating a SCIP binary and the individual libraries

SCIP uses the libraries of ZIMPL and SoPlex to be able to read ZIMPL models
and solve the subproblem LP relaxations with SoPlex.

In order to compile the whole bundle, just enter

    make

within the SCIP Optimization Suite main directory. If you are using a
gcc compiler with version less 4.2 you have to compile with the following
additional flags

    make LPSOPT=opt-gccold ZIMPLOPT=opt-gccold OPT=opt-gccold

If all goes well, you should get a final message

** Build complete.
** Find your binary in "<path>/scipoptsuite-<version>/scip-<version>/bin".
** Enter "make test" to solve a number of easy instances in order to verify that SCIP runs correctly.


If this is not the case, there are most probably some libraries missing on
your machine or they are not in the right version. In its default build, the
SCIP Optimization Suite needs the following external libraries:

- the Z Compression Library (ZLIB: `libz.a` or `libz.so` on Unix systems)
  Lets you read in `.gz` compressed data files.
- the GNU Multi Precision Library (GMP: `libgmp.a` or `libgmp.so` on Unix systems)
  Allows ZIMPL to perform calculations in exact arithmetic.
- the Readline Library (READLINE: `libreadline.a` or `libreadline.so` on Unix systems)
  Enables cursor keys and file name completion in the SCIP shell.

You can disable those packets using the following `make` arguments:
- `GMP=false`       (disables GMP support)
- `ZLIB=false`      (disables ZLIB support)
- `READLINE=false`  (disables READLINE support)

You can also disable ZIMPL by specifying `ZIMPL=false` as a `make` argument.
Note, however, that this disables the ZIMPL file reader in SCIP and you can
no longer read in ZIMPL models (the input files with a `.zpl` extension).

Since ZIMPL requires GMP, it is automatically disabled, if GMP is disabled.

Since the GMP is not installed on every machine, and the READLINE library is
sometimes only existing in an old version, these are the two most frequent
candidates for build problems. The following should work on most machines:

    make GMP=false READLINE=false

Note that on some MAC systems, GMP is installed under `/sw/include` and `/sw/lib`.
If these are not contained in the library and include paths, you have to add
them explicitly.

If this still does not work, you should try the following, which is the most
compatible method to build the SCIP Optimization Suite:

    make ZLIB=false GMP=false READLINE=false LPSOPT=opt-gccold OPT=opt-gccold

Note, however, that in this case, you cannot
- read compressed (`.gz`) input files from SCIP and SoPlex
- read ZIMPL models from SCIP
- use the counting of solutions feature exactly for large numbers of solutions
- use the ZIMPL binary to transform ZIMPL (`.zpl`) models into MIP instances
  of `.lp` or `.mps` type
- use the cursor keys and command line completion in SCIP


For more information how to install the components of the SCIP
Optimization Suite and for details concerning special architectures and
operating systems, see the INSTALL file of SCIP and the documentation
of each component.

## Creating a single library containing SCIP, SoPlex, and ZIMPL

In case you need a single library which contains the whole package, that is
SCIP, SoPlex, and ZIMPL, you can use the command:

    make scipoptlib

This will create a single library containing SCIP, SoPlex, and ZIMPL. It is
placed in the `lib` directory. This library is statically linked. If you
need a shared library use the command:

    make scipoptlib SHARED=true

## Creating GCG and UG

In case for the both SCIP extensions GCG and UG, you can easily compile
these using the commands

    make gcg
    make ug
