//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_SHARED_JOB_DATA_HPP
#define SAGA_ADAPTORS_CONDOR_JOB_SHARED_JOB_DATA_HPP

#include "pool_data.hpp"

#include <saga/saga/packages/job/job.hpp>

#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

#include <set>
#include <map>
#include <string>

namespace saga { namespace adaptors { namespace condor {

    struct shared_job_data
        : boost::enable_shared_from_this<shared_job_data>
    {
        typedef boost::recursive_mutex mutex;
        typedef boost::recursive_mutex::scoped_lock scoped_lock;
        typedef std::map<std::string, std::string> attribute_map;

        void register_job()
        {
            pool_->get_registry()->register_job(shared_from_this());
        }

        void unregister_job()
        {
            pool_->get_registry()->unregister_job(shared_from_this());
        }

        boost::shared_ptr<pool> pool_;
        saga::job::description description;
        saga::job::state state;

        attribute_map attributes;

        std::string full_job_id;
        std::string cluster_id;

        std::set<job_cpi_impl *> instances;

        boost::condition state_change;
        mutex state_change_mtx;
    };

    inline std::string const & get_cluster_id(
            boost::shared_ptr<shared_job_data> ptr)
    {
        return ptr->cluster_id;
    }

}}} // namespace saga::adaptors::condor

#endif // include guard
