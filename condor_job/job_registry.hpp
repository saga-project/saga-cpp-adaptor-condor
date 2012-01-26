//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_JOB_REGISTRY_HPP
#define SAGA_ADAPTORS_CONDOR_JOB_JOB_REGISTRY_HPP

#include <boost/shared_ptr.hpp>

#include <map>
#include <string>

namespace saga { namespace adaptors { namespace condor {

    struct shared_job_data;
    inline std::string const & get_cluster_id(
            boost::shared_ptr<shared_job_data>);

    //  Maps Condor job IDs to job instances. It is suggested that job IDs be
    //  either "Cluster" or "Cluster.Process".
    //  A registry should be maintained per Condor pool, for the benefit of the
    //  log processor.
    //
    //  NOTE: This is NOT thread-safe. Use with synchronized
    struct job_registry
    {
        ~job_registry();

        void register_job(boost::shared_ptr<shared_job_data> ptr);
        void unregister_job(boost::shared_ptr<shared_job_data> ptr);
        boost::shared_ptr<shared_job_data> find_job(std::string const & id);

    private:
        typedef std::map<std::string, boost::shared_ptr<shared_job_data> >
            job_map;
        job_map jobs_;
    };

}}} // namespace saga::adaptors::condor

#endif // include guard
