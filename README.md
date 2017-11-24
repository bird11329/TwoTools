# TwoTools
Two tools developed for management of all ROOT files using database (MySQL)
## 1. Introduction
For managing an increasing number of ROOT files of DMPSW is this project created.
This project, as implied by the caution implies, contains two components:
- SelectionTool: Display records (files) that meet the conditions provided if any
- OperationTool: Database handler for insert/update/delete (_Superusers only_)
The two components are designed for different users. For common users who would like only to find out desired files, "SelectionTool" could do. But for privileged users of the projects such as ROOT file producers, aside from their requirements for file catalog, they will also have to modify the records of the database,  in which case "OperationTool" stands and takes the responsibility.
## 2. Prerequisites
There are two external libraries required by this project.
- boost_program_options.
    This is a library from BOOST to parse the terminal arguments (options). BOOST is one of the most highly regarded and expertly designed C++ library projects in the world (<http://www.boost.org/>).
- mysqlclient.
    MySQL (database) support. This project uses MySQL to query a database.
Either library should be provided if one'd like to use this project by passing appropriate options to "configure", which constructs the compilation instructions such as Makefile or SConstruct. See the next chapter for more details.
Apart from the two external libraries, this project also requires some others:
- /dev/urandom
    Random string generator. The random strings are used only by privileged users to authenticate, while common users who need only SelectionTool to be installed don't have to worry about the absence of it.
- Network methods (headers), which include "netdb.h", "sys/socket.h", and "arpa/inet.h"
    To support DNS.
Fortunately, for the majority of Linux, the last two items are both available. For further revisions, the requirement for network methods could also be optional (to be controlled during "configure") rather than necessary.
## 3. Configure
As a project written in C++, it needs to be compiled. And before that, use "configure", a shell script, to generate a compilation guide. For users of ROOT (<http://root.cern.ch>), he/she could also find many copied lines from there.
For a direct view of it, try the option "-h,--help":
`$ ./configure -h`
Help message provided by "configure" will be printed. And for a quite start, only some options are introduced.
To compile the project, as mentioned above, two external libraries must be provided. And this is done by using option "-D" of "configure". On server in Geneva, the command below is tested:
`$ ./configure -DBOOST_LIB_PATH=/cvmfs/dampe.cern.ch/rhel6-64/externals/boost/1.55.0/lib/ -DBOOST_LIB_NAME=boost_program_options-mt`
Which only SelectionTool is desired. And for both tools, use
`$ ./configure -DBOOST_LIB_PATH=/cvmfs/dampe.cern.ch/rhel6-64/externals/boost/1.55.0/lib/ -DBOOST_LIB_NAME=boost_program_options-mt -o`
(The only difference between the two commands is the last option, "-o".)
Options from the two commands are explained as:
- \-DBOOST_LIB_PATH=/cvmfs/dampe.cern.ch/rhel6-64/externals/boost/1.55.0/lib/
    Path of external library "boost_program_options" is /cvmfs/dampe.cern.ch/rhel6-64/externals/boost/1.55.0/lib/
- \-DBOOST_LIB_NAME=boost_program_options-mt
    The exact name of "boost_program_options" is "boost_program_options-mt". Note it is **not** necessary to add the prefix "lib" nor the suffix ".so".
- \-o
    Build OperationTool. It won't be built without this option, which is the default setting.
(Here only boost_program_options is considered, because the default setting for mysqlcilent works.) If "configure" complete, it'll print in the last or penultimate line:
    `Configuration done.`
Or in case of failure, one shall correct the error(s) accordingly. Refer to "Guide for developers" for some help. It is *highly recommended to correct any errors confronted*, because a failed "configure" will probably lead to a compilation failure later if "configure" errors are ignored.
## 4. Compiling and verifying
If "Configuration done", one could just use
`$ make`
to compile the desired tool(s). And if nothing wrong, this project is ready for queries.
To test if it works, first, try the help option:
`$ ./SelectionTool -h`
`$ ./OperationTool -h`
And if the help message is printed, the program looks fine. Then try to test how database works by
`$ ./SelectionTool -o default.ini -f this_shall_never_exist`
This command asks SelectionTool to fetch a non-exist record. If no records are returned from database rather than some connection faults, SelectionTool is ready. And the exit status of this command if nothing wrong is also 0, as expected, even if no records are returned.
As for OperationTool, examples to test database connection are omitted here for security concerns. One can also turn to "Guide for developers".
-----
This all for this README. Thanks for your time and patience. If some bugs are found, please connect wangyp through an email (wangyp@pmo.ac.cn).
2017/11/24
