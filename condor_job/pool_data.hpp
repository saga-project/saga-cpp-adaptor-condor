//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_POOL_DATA_HPP
#define SAGA_ADAPTORS_CONDOR_JOB_POOL_DATA_HPP

#include "job_registry.hpp"
#include "synchronized.hpp"

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <string>

namespace saga { namespace adaptors { namespace condor {

    class job_cpi_impl;
    struct log_processor;
    struct temporary_file;

    struct pool
    {
        pool(std::string const & url, std::string const & log);
        ~pool();

        std::string const & get_log();

        std::string const & get_url() const
        {
            return url_;
        }

        synchronized<job_registry> & get_registry()
        {
            return registry_;
        }

    private:
        std::string const                   url_;
        std::string                         log_;
        synchronized<job_registry>          registry_;
        boost::scoped_ptr<temporary_file>   temp_log_;
        boost::scoped_ptr<log_processor>    log_processor_;
    };

}}} // namespace saga::adaptors::condor

#endif // include guard
