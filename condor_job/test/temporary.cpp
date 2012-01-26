//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "safe_getenv.hpp" // a mockup implementation

#include "../temporary.cpp"

#include <boost/assert.hpp>
// Boost.Filesystem
#include <saga/saga/adaptors/utils/filesystem.hpp>

#include <iostream>
#include <string>
#include <vector>

struct temporary_test
{
    typedef saga::adaptors::condor::temporary_file temporary_file;

    temporary_test(std::string const & templ = std::string())
        : template_(templ)
        , file_(saga::adaptors::condor::open_temporary_file(template_))
        , path_(file_->get_path())
    {
        std::cout << " ** Temporary file created (" << (void*) this << ")\n"
            "Filename template:  '" << template_ << "'\n"
            "Generated filename: '" << path_.string() << "'\n";
        run_test(true);

        BOOST_ASSERT(exists());
        BOOST_ASSERT(is_regular_file());
        BOOST_ASSERT(is_empty());
    }

    ~temporary_test()
    {
        file_.reset();

        std::cout << " ** Temporary file deleted (" << (void*) this << ")\n";
        run_test(false);

        BOOST_ASSERT(!exists());
    }

    bool exists() const
    {
        boost::system::error_code ec;
        boost::filesystem::file_status st = status(path_, ec);
        return boost::filesystem::exists(st);
    }

    bool is_regular_file() const
    {
        boost::system::error_code ec;
        boost::filesystem::file_status st = status(path_, ec);
        return boost::filesystem::is_regular_file(st);
    }

    bool is_empty() const
    {
        return boost::filesystem::is_empty(path_);
    }

    void run_test(bool want_it_to_exist = true)
    { 
        boost::system::error_code ec;
        boost::filesystem::file_status st = status(path_, ec);

        bool result = status_known(st);
        std::cout << "    status_known:   " << result << "\n";

        if (result)
        {
            result = boost::filesystem::exists(st);
            std::cout << "    exists:         " << result << "\n";
        }

        if (result)
            std::cout << "    is_regular_file:"
                << boost::filesystem::is_regular_file(st) << "\n"
                "    is_empty:       "
                << boost::filesystem::is_empty(path_) << "\n";
    }

private:
    temporary_test(temporary_test const &);
    temporary_test & operator=(temporary_test const &);

    std::string template_;
    std::auto_ptr<temporary_file> file_;
    boost::filesystem::path path_;
};

int main(int argc, char ** argv)
{
    std::cout << std::boolalpha;

    temporary_test tt
        , t1("my-own-template")
        , t2("./another-template")
        , t3(".")
        , t4("..")
        , t5("./")
        , t6("../")
        , t7("/tmp/");

    // TODO Add filename tests on the ones using relative paths
}

