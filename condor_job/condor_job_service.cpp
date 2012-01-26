//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "condor_job_service.hpp"
#include "condor_job_adaptor.hpp"
#include "helper.hpp"

#include <saga/saga/adaptors/attribute.hpp>
#include <saga/saga/packages/job/adaptors/job.hpp>

#include <boost/regex.hpp>

#include <set>

namespace saga { namespace adaptors { namespace condor {

    job_service_cpi_impl::job_service_cpi_impl(proxy * p, cpi_info const & info,
            saga::ini::ini const & glob_ini,
            saga::ini::ini const & adap_ini,
            TR1::shared_ptr<saga::adaptor> adaptor)
        : base_cpi(p, info, adaptor, cpi::Noflags)
    {
        saga::url const instance_rm = instance_data(this)->rm_;
        condor_url_ = get_adaptor()->validate_rm(instance_rm);

        // TODO: Should we try something here, like connecting to a GAHP server,
    }

    void job_service_cpi_impl::sync_create_job(saga::job::job & ret,
            saga::job::description jd)
    {
        using namespace saga::job::attributes;

        // make sure the executable path is given
        if (!jd.attribute_exists(description_executable)
                || jd.get_attribute(description_executable).empty())
            SAGA_ADAPTOR_THROW(
                "Missing 'Executable' attribute in job description.",
                saga::BadParameter);

        saga::url const instance_rm = instance_data(this)->rm_;
        saga::job::job job = saga::adaptors::job(instance_rm, jd,
                proxy_->get_session());

        // set the created attribute
        saga::adaptors::attribute jobattr (job);
        std::time_t current = 0;
        std::time(&current);
        jobattr.set_attribute(saga::job::attributes::created, ctime(&current));

        ret = job;
    }

    void job_service_cpi_impl::sync_get_job(saga::job::job & ret, std::string jobid)
    {
        saga::url rm;
        std::string id;

        try
        {
            detail::helper::parse_jobid(jobid, rm, id);
        }
        catch (saga::exception const & e)
        {
            SAGA_ADAPTOR_THROW("Can't parse job ID: (" + e.what() + ").",
                    saga::BadParameter);
        }

        saga::url const instance_rm = instance_data(this)->rm_;

        // Validation of backend URL: Are we supposed to handle this request?
        if (!instance_rm.get_string().empty())
        {
            bool url_mismatch = false;

            {
                std::string const scheme = instance_rm.get_scheme();
                if (!scheme.empty() || "any" != scheme)
                    if (scheme != rm.get_scheme())
                        url_mismatch = true;
            }

            {
                std::string const host = instance_rm.get_host();
                if (!host.empty())
                    //  FIXME:  Should we use Boost.Asio to compare hosts?
                    //          This would handle aliases, IP/hostnames, etc.
                    if (host != rm.get_host())
                        url_mismatch = true;
            }

            {
                int port = instance_rm.get_port();
                if (port != rm.get_port())
                    url_mismatch = true;
            }

            //  Not processing other parts of the URL here, for their relation
            //  to the user specified URL is not well-defined. E.g., a single
            //  Resource Manager may be able to handle different paths.
            //
            //      username
            //      password
            //      path
            //      query
            //      fragment

            if (url_mismatch)
                SAGA_ADAPTOR_THROW("Backend URL in job ID ('" + jobid + "')"
                        " does not match this saga::job::service's resource"
                        " manager URL ('" + instance_rm.get_url() + "').",
                        saga::BadParameter);
        }

        //  Could still compare the Backend URL to the configuration of the
        //  adaptor. Not doing it, so as to keep this general, which allows
        //  different job_cpis to fight for the job id.
        //
        //  For the same reasons, not checking format of the actual ID.
        //
        //  Further checks should go in job implementations.

        saga::job::job job = saga::adaptors::job(instance_rm,
                jobid, proxy_->get_session());

        //  This is a running job, by definition. If the job hasn't set a
        //  Created timestamp, we don't know any better, here.
        //
        // BOOST_ASSERT(job.attribute_exists(saga::job::attributes::created));

        ret = job;
    }

    void job_service_cpi_impl::sync_list(std::vector<std::string> & list_of_jobids)
    {
        try
        {
            std::vector<std::string> args;
            args.push_back("-format");
            args.push_back("%d.");
            args.push_back("ClusterId");
            args.push_back("-format");
            args.push_back("%d\\n");
            args.push_back("ProcId");

            // Might also want to get recent history, with condor_history, no?
            boost::process::child c =
                get_adaptor()->run_condor_command("condor_q", args,
                    boost::process::close_stream);

            boost::process::pistream & out = c.get_stdout();

            std::set<std::string> clusters;
            std::string process;
            while (getline(out, process))
            {
                static const boost::regex re("(\\d+)\\.\\d+");
                boost::smatch match;

                if (boost::regex_match(process, match, re))
                {
                    std::string cluster = match.str(1);
                    if (clusters.insert(cluster).second)
                        list_of_jobids.push_back(
                            "[" + condor_url_ + "]-[" + cluster + "]");

                    list_of_jobids.push_back(
                        "[" + condor_url_ + "]-[" + process + "]");
                }
            }

            boost::process::status status = c.wait();
        }
        catch (std::exception const & e)
        {
            SAGA_ADAPTOR_THROW("Problem retrieving list of condor jobs, "
                "std::exception caught: '" + e.what() + "'.",
                saga::BadParameter);
        }
    }

}}} // namespace saga::adaptors::condor
