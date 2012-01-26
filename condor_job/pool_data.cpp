//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "log_processor.hpp"
#include "pool_data.hpp"
#include "temporary.hpp"

namespace saga { namespace adaptors { namespace condor {

    // NOTE: We need the empty constructors and destructors here, to make sure
    // scoped_ptr<...> has complete definitions of log_processor and
    // temporary_file available and to avoid cyclical header dependencies.

    pool::pool(std::string const & url, std::string const & log)
        : url_(url), log_(log)
    {
    }

    pool::~pool() {}

    std::string const & pool::get_log()
    {
        // Hold the lock, to avoid opening multiple log-files and -processors.
        synchronized<job_registry>::lock lck(registry_);

        if (log_.empty())
        {
            temp_log_.reset(open_temporary_file("saga-condor-log").release());
            log_ = temp_log_->get_path().string();
        }
        // FIXME    We should somehow make sure we're not sharing logs across
        //          schedd's. The only job identifying information in the log is
        //          the Cluster ID :-/

        if (!log_processor_)
            log_processor_.reset(new log_processor(log_, registry_));

        return log_;
    }

}}} // namespace saga::adaptors::condor
