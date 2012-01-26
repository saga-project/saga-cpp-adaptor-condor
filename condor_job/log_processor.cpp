//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "classad.hpp"
#include "condor_job.hpp"
#include "log_processor.hpp"

#include <saga/saga-defs.hpp>

namespace saga { namespace adaptors { namespace condor {

    void log_processor::process_log_entry(::condor::job::classad & c)
    {
        using namespace saga::job::attributes;

        struct logger
        {
            logger(::condor::job::classad & ca)
                : ca_(ca), processed(false)
            {
            }

            ~logger()
            {
                if (processed)
                    return;

                // Log entry
                std::string msg;

                SAGA_VERBOSE(SAGA_VERBOSE_LEVEL_INFO)
                    msg += "Skipping unprocessed Condor log entry.";
                SAGA_VERBOSE(SAGA_VERBOSE_LEVEL_DEBUG)
                {
                    msg += "ClassAd {\n";
                    for (::condor::job::classad::attribute_iterator
                            iter = ca_.attributes_begin(),
                            end = ca_.attributes_end();
                            iter != end; ++iter)
                    {
                        msg += "  " + iter->first
                            + ": (" + iter->second.type + ") "
                            "'" + iter->second.value_ + "'\n";
                    }
                    msg += "}";
                }
                if (!msg.empty())
                    SAGA_LOG_INFO(msg.c_str())
            }

            ::condor::job::classad & ca_;
            bool processed;
        }
        _on_return(c);

        int event_type = -1;
        std::string cluster;
        std::string process;

        boost::optional< ::condor::job::classad::value> attr;

        if ((attr = c.get_attribute("EventTypeNumber"))
                && "i" == attr->type)
            event_type = boost::lexical_cast<int>(attr->value_);
        if (!(attr = c.get_attribute("Cluster")))
            return;
        cluster = attr->value_;
        if ((attr = c.get_attribute("Proc")))
            process = attr->value_;

        boost::shared_ptr<shared_job_data> job_data;
        {
            synchronized<job_registry>::lock reg(registry_);
            job_data = reg->find_job(cluster);
            if (!job_data)
                job_data = reg->find_job(cluster + "." + process);
        }

        if (!job_data)
            return;

        shared_job_data::scoped_lock lock(job_data->state_change_mtx);

        // Event descriptions from section 2.6.6 of the Condor manual.
        // E.g., from here:
        // http://www.cs.wisc.edu/condor/manual/v7.1.0/2_6Managing_Job.html

        switch (event_type)
        {
        case 0:     //  Job submitted
        //  This event occurs when a user submits a job.  It is the
        //  first event you will see for a job, and it should only occur
        //  once.
        case 1:      // Job executing
        //  This shows up when a job is running. It might occur more
        //  than once.
        case 13:     // Job was released
        //  The user is requesting that a job on hold be re-run.

            job_data->state = saga::job::Running;
            break;

        case 12:     // Job was held
        //  The user has paused the job, perhaps with the condor_ hold
        //  command. It was stopped, and will go back into the queue
        //  again until it is aborted or released.

            job_data->state = saga::job::Suspended;
            break;

        case 2:      // Error in executable
        //  The job couldn't be run because the executable was bad.
        case 4:      // Job evicted from machine
        //  A job was removed from a machine before it finished, usually
        //  for a policy reason: perhaps an interactive user has claimed
        //  the computer, or perhaps another job is higher priority.

            if ((attr = c.get_attribute("EventTime")))
                job_data->attributes[finished] = attr->value_;

            job_data->state = saga::job::Failed;
            break;

        case 5:      // Job terminated
        //  The job has completed.

            if ((attr = c.get_attribute("EventTime")))
                job_data->attributes[finished] = attr->value_;
            if ((attr = c.get_attribute("ReturnValue")))
                job_data->attributes[exitcode] = attr->value_;

            job_data->state = saga::job::Done;

            if ((attr = c.get_attribute("TerminatedBySignal")))
            {
                job_data->attributes[termsig] = attr->value_;
                job_data->state = saga::job::Failed;
            }

            break;

        case 9:      // Job aborted
        //  The user cancelled the job.

            if ((attr = c.get_attribute("EventTime")))
                job_data->attributes[finished] = attr->value_;

            job_data->state = saga::job::Canceled;
            break;

        case 3:      // Job was checkpointed
        //  The job's complete state was written to a checkpoint file.
        //  This might happen without the job being removed from a
        //  machine, because the checkpointing can happen periodically.

        case 6:      // Image size of job updated
        //  This is informational. It is referring to the memory that
        //  the job is using while running. It does not reflect the
        //  state of the job.

        case 7:      // Shadow exception
        //  The condor_shadow, a program on the submit computer that
        //  watches over the job and performs some services for the job,
        //  failed for some catastrophic reason. The job will leave the
        //  machine and go back into the queue.

        case 8:      // Generic log event

        case 10:     // Job was suspended
        //  The job is still on the computer, but it is no longer
        //  executing. This is usually for a policy reason, like an
        //  interactive user using the computer.

        case 11:     // Job was unsuspended
        //  The job has resumed execution, after being suspended
        //  earlier.

        case 14:     // Parallel node executed
        //  A parallel (MPI) program is running on a node.

        case 15:     // Parallel node terminated
        //  A parallel (MPI) program has completed on a node.

        case 16:     // POST script terminated
        //  A node in a DAGMan workflow has a script that should be run
        //  after a job. The script is run on the submit host.  This
        //  event signals that the post script has completed.

        case 17:     // Job submitted to Globus
        //  A grid job has been delegated to Globus (version 2, 3, or
        //  4).

        case 18:     // Globus submit failed
        //  The attempt to delegate a job to Globus failed.

        case 19:     // Globus resource up
        //  The Globus resource that a job wants to run on was
        //  unavailable, but is now available.

        case 20:     // Detected Down Globus Resource
        //  The Globus resource that a job wants to run on has become
        //  unavailable.

        case 21:     // Remote error
        //  The condor_starter (which monitors the job on the execution
        //  machine) has failed.

        case 22:     // Remote system call socket lost
        //  The condor_shadow and condor_starter (which communicate
        //  while the job runs) have lost contact.

        case 23:     // Remote system call socket reestablished
        //  The condor_shadow and condor_starter (which communicate
        //  while the job runs) have been able to resume contact before
        //  the job lease expired.

        case 24:     // Remote system call reconnect failure
        //  The condor_shadow and condor_starter (which communicate
        //  while the job runs) were unable to resume contact before the
        //  job lease expired.

        case 25:     // Grid Resource Back Up
        //  A grid resource that was previously unavailable is now
        //  available.

        case 26:     // Detected Down Grid Resource
        //  The grid resource that a job is to run on is unavailable.

        case 27:     // Job submitted to grid resource
        //  A job has been submitted, and is under the auspices of the
        //  grid resource.

        default:
            return;
        }

        std::set<job_cpi_impl *>::iterator end = job_data->instances.end();
        for (std::set<job_cpi_impl *>::iterator it = job_data->instances.begin();
             it != end; ++it)
        {
            (*it)->update_state(job_data->state, job_data->attributes);
        }
        job_data->state_change.notify_all();

        _on_return.processed = true;
    }

}}} // namespace saga::adaptors::condor
