//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "classad.hpp"
#include "condor_job.hpp"
#include "condor_job_adaptor.hpp"
#include "description.hpp"
#include "helper.hpp"
#include "status.hpp"

#include <saga/saga-defs.hpp>

#include <boost/bind.hpp>
#include <boost/process.hpp>
#include <boost/regex.hpp>

///////////////////////////////////////////////////////////////////////////////
//  this is a hack to make boost::process work (avoid multiple definitions)
//  (this will go away as soon as it is fixed in Boost.Process)
#if defined(BOOST_PROCESS_WIN32_API)
const boost::process::detail::file_handle::handle_type 
    boost::process::detail::file_handle::INVALID_VALUE = INVALID_HANDLE_VALUE;
#endif

namespace saga { namespace adaptors { namespace condor {

    job_cpi_impl::~job_cpi_impl()
    {
        // No more notifications for us, we're out of here.
        shared_job_data::scoped_lock lock(job_data_->state_change_mtx);
        job_data_->instances.erase(this);
    }

    void job_cpi_impl::set_job_id()
    {
        using namespace saga::job::attributes;

        {
            saga::adaptors::attribute attr(this);
            BOOST_ASSERT(((!attr.attribute_exists(jobid)
                        || attr.attribute_exists(jobid)
                        && attr.get_attribute(jobid).empty())
                    || job_data_->full_job_id.empty())
                && "Error: Tried to reset previously set job ID.");

            attr.set_attribute(jobid, job_data_->full_job_id);
        }

        if (!job_data_->full_job_id.empty()
                && job_data_.unique())
            job_data_->register_job();
    }

    job_cpi_impl::job_cpi_impl(proxy * p, cpi_info const & info,
            saga::ini::ini const & glob_ini,
            saga::ini::ini const & adap_ini,
            TR1::shared_ptr<saga::adaptor> adaptor)
        : base_cpi(p, info, adaptor, cpi::Noflags)
        , proxy_lock_(p->shared_from_this())
        , state_changed_(false)
        , cached_state_(saga::job::New)
    {
        using namespace saga::job::attributes;

        saga::job::state state;
        {
            saga::monitorable monitor(this->proxy_);
            saga::metric m(monitor.get_metric(saga::metrics::task_state));

            state = saga::adaptors::job_state_value_to_enum(
                m.get_attribute(saga::attributes::metric_value));
        }

        // This should only happen if this is not the first CPI instance that
        // manipulates the object.
        if (state != saga::job::Unknown && state != saga::job::New)
            SAGA_ADAPTOR_THROW("Job has been started already!",
                saga::IncorrectState);

        {
            instance_data data(this);

            if (data->init_from_jobid_)
            {
                std::string rm;
                std::string id;

                try
                {
                    saga::url rm_url;
                    detail::helper::parse_jobid(data->jobid_, rm_url, id);

                    rm = rm_url.get_string();
                }
                catch (saga::exception const & e)
                {
                    SAGA_ADAPTOR_THROW("Can't parse job ID: (" + e.what()
                        + ").", saga::BadParameter);
                }

                job_data_ = get_adaptor()->find_job(rm, id);
                if (!job_data_)
                {
                    job_data_.reset(new shared_job_data());

                    std::string output;

                    try
                    {
                        std::vector<std::string> args;
                        args.push_back("-xml");
                        args.push_back(id);

                        boost::process::child c =
                            get_adaptor()->run_condor_command("condor_q",
                                args, boost::process::close_stream);

                        boost::process::pistream & out = c.get_stdout();
                        for (std::string line; getline(out, line);
                                output += line + "\n")
                            /* Nothing to do */;

                        // Ignoring exit status. Will try to process output and
                        // see if we can get the job's ClassAd.
                        c.wait();
                    }
                    catch (std::exception const & e)
                    {
                        SAGA_ADAPTOR_THROW("Couldn't get job status for job ID "
                                + data->jobid_ + ": std::exception caught: " +
                                e.what() + ".", saga::BadParameter);
                    }

                    // Parse classad from condor_q
                    ::condor::job::classad ca;
                    if (!ca.find_and_parse(output))
                        SAGA_ADAPTOR_THROW("Job not found: " + data->jobid_
                            + ".", saga::DoesNotExist);

                    {
                        boost::optional< ::condor::job::classad::value>
                            status  = ca.get_attribute("JobStatus"),
                            log     = ca.get_attribute("UserLog"),
                            log_xml = ca.get_attribute("UserLogUseXML");

                        job_data_->state = status
                            ? ::condor::job::status(status->value_)
                            : saga::job::Running;   // assume a running job

                        if (log && !log->value_.empty()
                                && log_xml && "t" == log_xml->value_)
                        {
                            job_data_->pool_ = get_adaptor()->get_pool(
                                    rm, log->value_);

                            // Start log processing
                            job_data_->pool_->get_log();
                        }
                        else
                            job_data_->pool_ = get_adaptor()->get_pool(
                                    rm, "//-- No XML Log --//");
                    }

                    job_data_->cluster_id = id;
                    job_data_->full_job_id = data->jobid_;
                    job_data_->description = detail::condor_to_saga(ca);
                }

                data->jd_ = job_data_->description;
                data->jd_is_valid_ = true;
            }
            else // !data->init_from_jobid_
            {
                job_data_.reset(new shared_job_data());
                job_data_->state = saga::job::New;
                job_data_->pool_ = get_adaptor()->get_pool(
                        data->rm_.get_string());

                if (!data->jd_is_valid_)
                    SAGA_ADAPTOR_THROW("Job description cannot be retrieved.",
                        saga::IncorrectState);

                // Job description validation

                if (!data->jd_.attribute_exists(description_executable))
                    SAGA_ADAPTOR_THROW("Executable attribute not specified.",
                        saga::BadParameter);

                if (data->jd_.attribute_exists(description_interactive)
                        && data->jd_.get_attribute(description_interactive)
                            == "True")
                    SAGA_ADAPTOR_THROW("Interactive execution not implemented.",
                        saga::NotImplemented);
            }
        }

        set_job_id();

        {
            // Update status and start receiving events
            shared_job_data::scoped_lock lock(job_data_->state_change_mtx);

            update_state(job_data_->state, job_data_->attributes);
            job_data_->instances.insert(this);
        }
    }

    void job_cpi_impl::sync_get_state(saga::job::state& state)
    {
        state = get_state();
    }

    void job_cpi_impl::sync_get_description(saga::job::description& jd)
    {
        instance_data data(this);
        BOOST_ASSERT(data->jd_is_valid_);
        jd = data->jd_.clone();     // return a deep copy of the job description
    }

    void job_cpi_impl::sync_get_job_id(std::string& jobid)
    {
        jobid = get_job_id();
    }

    void job_cpi_impl::sync_run(saga::impl::void_t&)
    {
        // FIXME: What happens if multiple threads try to run job at the same
        // time? Need some kind of lock here.

        // Verify job hasn't been started.
        if (saga::job::New != get_state())
            SAGA_ADAPTOR_THROW("Job has been started already!",
                    saga::IncorrectState);
        if (!job_data_->cluster_id.empty())
            SAGA_ADAPTOR_THROW("Condor cluster ID has already been set.",
                    saga::IncorrectState);

        std::vector<std::string> args;
        args.push_back("-append");
        args.push_back("log = " + job_data_->pool_->get_log());
        args.push_back("-append");
        args.push_back("log_xml = True");

        std::string output;

        // Prevent processing of events, so we don't lose any...
        synchronized<job_registry>::lock lck(job_data_->pool_->get_registry());

        try
        {
            ::condor::job::description desc;
            {
                instance_data data(this);

                BOOST_ASSERT(data->jd_is_valid_);
                desc = detail::saga_to_condor(data->jd_,
                        data->rm_,
                        this->proxy_->get_session().list_contexts(),
                        get_adaptor()->get_default_job_attributes());
            }

            boost::process::child c =
                get_adaptor()->run_condor_command("condor_submit", args);
            args.clear();

            boost::process::postream & in = c.get_stdin();
            boost::process::pistream & out = c.get_stdout();

            std::stringstream os;
                os << " ** Condor adaptor (job::run)\n"
                    "    About to submit job description:\n"
                    "========================================\n"
                    << desc
                    << "========================================\n";
            
            SAGA_LOG_DEBUG(os.str());

            in << desc << std::flush;
            in.close();

            for (std::string line; getline(out, line); output += "\n  " + line)
                /* Nothing to do */;

            boost::process::status status = c.wait();
            if (!status.exited() || status.exit_status())
                SAGA_ADAPTOR_THROW("Failed to submit job to condor pool. "
                    "Output from condor_submit follows:\n" + output,
                    saga::NoSuccess);
        }
        catch (saga::adaptors::exception const &)
        {
            // Let our exceptions fall through.
            throw;
        }
        catch (std::exception const & e)
        {
            SAGA_ADAPTOR_THROW("Problem launching condor job: "
                "(std::exception caught: " + e.what() + ")",
                saga::BadParameter);
        }

        shared_job_data::scoped_lock lock(job_data_->state_change_mtx);
        job_data_->state = saga::job::Running;

        static const boost::regex re(
            "^  \\d+ job\\(s\\) submitted to cluster (\\d+).");
        boost::smatch match;
        if (regex_search(output, match, re))
            job_data_->cluster_id = match.str(1);
        else
        {
            // Job submission was successful and job should have started, since
            // condor_submit exited normally. Somehow, we failed to grab Cluster
            // ID from the output of condor_submit.
            // If we throw, other adaptors would be attempted :-/
            std::string msg = "Failed to determine Cluster ID from the output "
                "of condor_submit (see below). Won't be able to perform "
                "further operations on the job.\n" + output;
            SAGA_LOG_WARN(output.c_str())

            job_data_->cluster_id = "Unknown";
        }

        job_data_->full_job_id = std::string("[") + job_data_->pool_->get_url()
            + "]-[" + job_data_->cluster_id + "]";

        set_job_id();
        update_state(job_data_->state, job_data_->attributes);
    }

    void job_cpi_impl::sync_cancel(saga::impl::void_t&, double timeout)
    {
        // Verify the job has started and we have a valid cluster ID for it.
        // No point in verifying the job is actually running, since it would
        // always be a race condition.
        if (saga::job::New == get_state())
            SAGA_ADAPTOR_THROW("Can't cancel a job that hasn't started.",
                saga::IncorrectState);
        if (job_data_->cluster_id.empty())
            SAGA_ADAPTOR_THROW("Condor cluster ID is not known. "
                "Can't cancel job.", saga::IncorrectState);

        // Invoking sub-process takes time
        boost::xtime t;
        boost::xtime_get(&t, boost::TIME_UTC);

        try
        {
            std::vector<std::string> args;
            args.push_back(job_data_->cluster_id);
            boost::process::child c =
                get_adaptor()->run_condor_command("condor_rm", args,
                    boost::process::close_stream);

            std::string output;
            boost::process::pistream & out = c.get_stdout();
            for (std::string line; getline(out, line); output += "\n  " + line)
                /* Nothing to do */;
            boost::process::status status = c.wait();
            if (!status.exited() || status.exit_status())
                SAGA_ADAPTOR_THROW("Failed to cancel condor job "
                    "[" + job_data_->cluster_id + "]. "
                    "Output from condor_rm follows:\n" + output,
                    saga::NoSuccess);
        }
        catch (saga::adaptors::exception const &)
        {
            // Let our exceptions fall through.
            throw;
        }
        catch (std::exception const & e)
        {
            SAGA_ADAPTOR_THROW("Problem cancelling condor job: "
                "(std::exception caught: " + e.what() + ")",
                saga::BadParameter);
        }

        if (0. == timeout || is_state_final())
            return;

        shared_job_data::scoped_lock lock(job_data_->state_change_mtx);
        if (0. < timeout)
        {
            // TODO Allow for sub-second resolution where supported.
            t.sec += static_cast<boost::xtime::xtime_sec_t>(timeout);

            job_data_->state_change.timed_wait(lock, t,
                boost::bind(&job_cpi_impl::is_state_final, this));
        }
        else // 0. > timeout
            job_data_->state_change.wait(lock,
                boost::bind(&job_cpi_impl::is_state_final, this));
    }

    void job_cpi_impl::sync_wait(bool & finished, double timeout)
    {
        // Verify the job has started and we have a valid cluster ID for it.
        // No point in verifying the job is actually running, since it would
        // always be a race condition.
        if (saga::job::New == get_state())
            SAGA_ADAPTOR_THROW("Can't wait on a job that hasn't started.",
                saga::IncorrectState);
        if (job_data_->cluster_id.empty())
            SAGA_ADAPTOR_THROW("Condor cluster ID is not known. "
                "Can't wait on job.", saga::IncorrectState);

        if ((finished = is_state_final()) || 0. == timeout)
            return;

        shared_job_data::scoped_lock lock(job_data_->state_change_mtx);
        if (0. < timeout)
        {
            boost::xtime t;
            boost::xtime_get(&t, boost::TIME_UTC);

            // TODO Allow for sub-second resolution where supported.
            t.sec += static_cast<boost::xtime::xtime_sec_t>(timeout);

            finished = job_data_->state_change.timed_wait(lock, t,
                boost::bind(&job_cpi_impl::is_state_final, this));
        }
        else // 0. > timeout
        {
            job_data_->state_change.wait(lock,
                boost::bind(&job_cpi_impl::is_state_final, this));
            finished = true;
        }
    }

    void job_cpi_impl::sync_suspend(saga::impl::void_t&)
    {
        // Verify the job has started and we have a valid cluster ID for it.
        // No point in verifying the job is actually running, since it would
        // always be a race condition.
        if (saga::job::New == get_state())
            SAGA_ADAPTOR_THROW("Can't suspend a job that hasn't started.",
                saga::IncorrectState);
        if (job_data_->cluster_id.empty())
            SAGA_ADAPTOR_THROW("Condor cluster ID is not known. "
                "Can't suspend job.", saga::IncorrectState);

        // TODO
        SAGA_ADAPTOR_THROW("Suspending jobs not implemented.",
            saga::NotImplemented);
    }

    void job_cpi_impl::sync_resume(saga::impl::void_t&)
    {
        // Verify the job is suspended and we have a valid cluster ID for it.
        if (saga::job::Suspended != get_state())
            SAGA_ADAPTOR_THROW("Can't resume a job that isn't suspended.",
                saga::IncorrectState);
        if (job_data_->cluster_id.empty())
            SAGA_ADAPTOR_THROW("Condor cluster ID is not known. "
                "Can't resume job.", saga::IncorrectState);

        // TODO
        SAGA_ADAPTOR_THROW("Resuming jobs not implemented.",
            saga::NotImplemented);
    }

}}} // namespace saga::adaptors::condor
