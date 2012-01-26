//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../job_registry.cpp"
#include "../synchronized.hpp"
#include "../log_processor.cpp"

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>

int main(int argc, char const ** argv)
{
    using saga::adaptors::condor::log_processor;
    using saga::adaptors::condor::job_registry;
    using saga::adaptors::condor::synchronized;

    // Test mode
    if (argc == 1)
    {
        static char const * alternate_argv[] = { argv[0]
                , "saga-condor-log.classad"
            };

        argc = sizeof(alternate_argv)/sizeof(alternate_argv[0]);
        argv = alternate_argv;
    }

    if (argc < 2)
    {
        std::cout << "Logfile not specified.\n"
            << std::flush;
        return 1;
    }

    boost::xtime t;
    boost::xtime_get(&t, boost::TIME_UTC);

    synchronized<job_registry> registry;
    std::vector<boost::shared_ptr<log_processor> > logs;
    for (int i = 1; i < argc; ++i)
    {
        std::cout << "Queueing log file: " << argv[i] << std::endl;
        boost::shared_ptr<log_processor> p(new log_processor(argv[i], registry));
        logs.push_back(p);
    }

    t.sec += 1;
    boost::thread::sleep(t);
}
