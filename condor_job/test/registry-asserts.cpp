//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_ENABLE_ASSERT_HANDLER

#include "../job_registry.cpp"

#include <iostream>
#include <string>

struct assertion {};

namespace boost {

    void assertion_failed(char const * expr, char const * function,
            char const * file, long line)
    {
        std::cout << "Assertion FAILED:\n"
            "  File:       " << file << "\n"
            "  Line:       " << line << "\n"
            "  Function:   " << function << "\n"
            "  Expression: (" << expr << ")\n"
            << std::flush;

        throw assertion();
    }

} // namespace boost

struct test_monitor
{
    test_monitor()
        : total(0)
        , passed(0)
        , failed(0)
    {
    }

    ~test_monitor()
    {
        std::cout <<
            "  ======================================"
            "======================================\n"
            "    " << total << " tests, " << passed << " passed, " << failed
            << " failed.\n"
            "  ======================================"
            "======================================\n"
            << std::flush;

        exit(failed);
    }

    int total;
    int passed;
    int failed;
} _test;

#define EXPECT_ASSERTION(description, block)                                \
    do                                                                      \
    {                                                                       \
        ++::_test.total;                                                    \
        std::cout << "Test #" << ::_test.total << ": " #description "\n"    \
            "  => Expecting an assertion\n" << std::flush;                  \
                                                                            \
        try                                                                 \
        {                                                                   \
            {                                                               \
                block                                                       \
            }                                                               \
            ++::_test.failed;                                               \
            std::cout << "**** Test FAILED!\n" << std::flush;               \
        }                                                                   \
        catch (assertion const &)                                           \
        {                                                                   \
            ++::_test.passed;                                               \
            std::cout << "---- Test passed!\n" << std::flush;               \
        }                                                                   \
    }                                                                       \
    while (false)                                                           \
    /**/

namespace saga { namespace adaptors { namespace condor {

    inline std::string const & get_cluster_id(shared_job_data * ptr)
    {
        static std::string result;
        switch ((intptr_t)ptr)
        {
            case 1: result = "test-job-1";
                break;
            case 2: result = "test-job-2";
                break;
            default: result = "unknown-job";
        }

        return result;
    }

}}} // namespace saga::adaptors::condor

int main()
{
    using namespace saga::adaptors::condor;

    {
        job_registry registry;
        boost::shared_ptr<shared_job_data> p1(new shared_job_data())
            , p2(new shared_job_data())
            , p1_(new shared_job_data());

        p1->cluster_id = "fictional-job-1";
        p2->cluster_id = "fictional-job-2";
        p1_->cluster_id = "fictional-job-1";

        registry.register_job(p1);
        registry.register_job(p2);

        EXPECT_ASSERTION("Double registration",
            registry.register_job(p1); );
        EXPECT_ASSERTION("Double registration",
            registry.register_job(p1_); );

        registry.unregister_job(p1);

        EXPECT_ASSERTION("Unregister job twice",
            registry.unregister_job(p1); );

        registry.unregister_job(p2);
    }

    EXPECT_ASSERTION("Registered jobs remain on registry destruction",
        job_registry registry;

        boost::shared_ptr<shared_job_data> ptr(new shared_job_data());
        ptr->cluster_id = "fictional-job-1";
        ptr->instances.insert(0);
        registry.register_job(ptr);
    );
}

