//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "job_registry.hpp"
#include "shared_job_data.hpp"

#include <boost/assert.hpp>
#include <boost/version.hpp>

// Backporting for Boost < 1.35
#if !defined(BOOST_VERIFY)

# if defined(BOOST_DISABLE_ASSERTS)
#   define BOOST_VERIFY(expr) ((void) (expr))
# else
#   define BOOST_VERIFY(expr) BOOST_ASSERT(expr)
# endif

# define SAGA_ADAPTORS_CONDOR_UNDEFINE_BOOST_VERIFY

#endif

namespace saga { namespace adaptors { namespace condor {

    job_registry::~job_registry()
    {
        // Once registered, jobs are not removed from the registry. Make
        // sure registered jobs have no live instances.
        job_map::const_iterator end = jobs_.end();
        for (job_map::const_iterator it = jobs_.begin(); it != end; ++it)
        {
            shared_job_data::scoped_lock lock((*it).second->state_change_mtx);
            BOOST_ASSERT((*it).second->instances.empty()
                && "Registered CPI instances remain on registry destruction.");
        }
    }

    void job_registry::register_job(boost::shared_ptr<shared_job_data> ptr)
    {
        typedef job_map::mapped_type mapped_type;
        mapped_type & job = jobs_[get_cluster_id(ptr)];

        BOOST_ASSERT(ptr != job
            && "Double registration: job is already registered.");
        BOOST_ASSERT(!job
            && "Duplicate job ID: Another job has registered this ID.");

        job = ptr;
    }

    void job_registry::unregister_job(boost::shared_ptr<shared_job_data> ptr)
    {
        BOOST_VERIFY(jobs_.erase(get_cluster_id(ptr))
            && "Attempt to unregister job that is not registered.");
    }

    boost::shared_ptr<shared_job_data>
    job_registry::find_job(std::string const & id)
    {
        typedef job_map::iterator iterator;
        iterator iter = jobs_.find(id);
        if (jobs_.end() != iter)
            return iter->second;

        return boost::shared_ptr<shared_job_data>();
    }

}}} // namespace saga::adaptors::condor
