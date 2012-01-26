//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//
// This test is intended for submission on localhost, for now.
//

#include <saga/saga.hpp>

#include <boost/thread.hpp>

#include <cstring>
#include <cstdlib>

int main(int argc, char const ** argv)
{
    using namespace std;
    using namespace saga;
    using namespace saga::job::attributes;

    boost::xtime t;
    boost::xtime_get(&t, boost::TIME_UTC);

    bool spawning_test_mode = false;

    // Test mode
    if (argc < 2)
    {
        std::cout << "Command not specified. Using self in test mode.\n"
            << std::flush;

        static char const * alternate_argv[] = { argv[0],
            argv[0], "--test-mode" };

        argc = sizeof(alternate_argv)/sizeof(alternate_argv[0]);
        argv = alternate_argv;

        spawning_test_mode = true;
    }
    else if (2 == argc
            && !strcmp("--test-mode", argv[1]))
    {
        std::cout << "Condor-demo running in test-mode.\n\n"
            " => Waiting to be killed.\n"
            << std::flush;

        // Waiting to be killed
        t.sec += 60;
        boost::thread::sleep(t);

        return 42;
    }

    job::service js("condor://localhost"); 

    job::description jd;
    jd.set_attribute(description_executable, argv[1]);

    if (argc > 2)
    {
        vector<string> args;
        for (int i = 2; i < argc; ++i)
            args.push_back(argv[i]);
        jd.set_vector_attribute(description_arguments, args);
    }

    if (spawning_test_mode)
    {
        vector<string> envs;
        if (std::getenv("SAGA_LOCATION"))
            envs.push_back("SAGA_LOCATION=$ENV(SAGA_LOCATION)");

        //
        // We need our shared libraries. These also have to be added to the
        // condor_adaptor configuration.
        //

        // Mac
        if (std::getenv("DYLD_LIBRARY_PATH"))
            envs.push_back("DYLD_LIBRARY_PATH=$ENV(DYLD_LIBRARY_PATH)");
        // Linux
        if (std::getenv("LD_LIBRARY_PATH"))
            envs.push_back("LD_LIBRARY_PATH=$ENV(LD_LIBRARY_PATH)");
        // Windows (?)
        if (std::getenv("PATH"))
            envs.push_back("PATH=$ENV(PATH)");

        jd.set_vector_attribute(description_environment, envs);
    }

    // jd.set_attribute(description_input, "input");
    jd.set_attribute(description_output, "output");
    jd.set_attribute(description_error, "error");

    job::job job = js.create_job(jd);

    try
    {
        job.run();
    }
    catch (saga::exception const & e)
    {
        std::cerr << "Unable to start Condor job on condor://localhost/.\n\n"
            " ** Is the Condor adaptor configured for this machine?\n\n"
            "Error message follows:\n"
            << e.what();

        // Returning 0 so the test doesn't fail where the adaptor isn't
        // installed/configured :-/
        return 0;
    }

    std::cout << "jobid is: "
        << job.get_attribute(saga::job::attributes::jobid) << std::endl;
    std::cout << "jobid is: " << job.get_job_id() << std::endl;

    // If the spawned --test-mode process doesn't actually go into
    // waiting-to-be-killed-mode, the different sleep times below should catch
    // that :-)
    for (std::size_t i = (spawning_test_mode ? 0 : 10); i < 15; ++i)
    {
        std::cout << '.' << std::flush;

        ++t.sec;
        boost::thread::sleep(t);
    }
    std::cout << '\n' << std::flush;

    job.cancel(-1.0);

    std::cout << "Haha! Job cancelled!\n" << std::flush;
}

