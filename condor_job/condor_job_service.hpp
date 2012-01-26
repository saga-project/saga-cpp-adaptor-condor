//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_CONDOR_JOB_SERVICE_HPP
#define SAGA_ADAPTORS_CONDOR_JOB_CONDOR_JOB_SERVICE_HPP

#include "condor_job_adaptor.hpp"

#include <saga/impl/packages/job/job_service_cpi.hpp>

namespace saga { namespace adaptors { namespace condor {

    class job_service_cpi_impl
        : public saga::adaptors::v1_0::job_service_cpi<job_service_cpi_impl>
    {
        typedef saga::adaptors::v1_0::job_service_cpi<job_service_cpi_impl>
            base_cpi;

        TR1::shared_ptr<job_adaptor> get_adaptor() const
        {
            return TR1::static_pointer_cast<job_adaptor>(
                base_cpi::get_adaptor());
        }

        std::string condor_url_;

    public:
        job_service_cpi_impl(proxy * p, cpi_info const & info,
                saga::ini::ini const & glob_ini,
                saga::ini::ini const & adap_ini,
                TR1::shared_ptr<saga::adaptor> adaptor);

        // synchronous operations
        void sync_create_job(saga::job::job & ret, saga::job::description jd);
        void sync_get_job(saga::job::job & job, std::string jobid);

        void sync_list(std::vector<std::string> & list_of_jobids);

        // WONTFIX: In general, there should be no way to manage a Condor job as
        //          one (using Condor interfaces) from inside the job.
        //
        //     void sync_get_self(saga::job::self & self);

        // WONTFIX: Rely on fallback implementations in the engine; job_cpi
        //          provides remaining required bits.
        //
        //     void sync_run_job_noio(saga::job::job & ret, std::string commandline,
        //             std::string host);
        //     void sync_run_job(saga::job::job & ret, std::string commandline,
        //             std::string host, saga::job::ostream& in,
        //             saga::job::istream& out, saga::job::istream& err);
    };

}}} // namespace saga::adaptors::condor

#endif // include guard
