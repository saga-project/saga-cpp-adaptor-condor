//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "condor_job_adaptor.hpp"

#include "condor_job_service.hpp"
#include "condor_job.hpp"

#include <boost/algorithm/string/case_conv.hpp>

namespace saga { namespace adaptors { namespace condor {

    SAGA_ADAPTOR_REGISTER(job_adaptor)

    saga::impl::adaptor_selector::adaptor_info_list_type
    job_adaptor::adaptor_register(saga::impl::session * s)
    {
        saga::impl::adaptor_selector::adaptor_info_list_type list;
        preference_type prefs;

        job_cpi_impl::register_cpi(list, prefs, adaptor_uuid_);
        job_service_cpi_impl::register_cpi(list, prefs, adaptor_uuid_);

        return list;
    }

    bool job_adaptor::init(saga::impl::session *,
            saga::ini::ini const& glob_ini, saga::ini::ini const& adap_ini)
    {
        BOOST_ASSERT(!initialized_);

        typedef std::pair<std::string const, std::string> entry;

        //default_rm_ = validate_rm(adap_ini.get_entry("resource_manager", ""));

        // Default settings for Job classad. Settings defined on the application
        // override the ones here.
        std::string default_ =
            adap_ini.get_entry("default_attributes", "default_attributes");
        if (!default_.empty() && adap_ini.has_section_full(default_))
        {
            saga::ini::ini default_section = adap_ini.get_section(default_);

            typedef saga::ini::entry_map entry_map;

            entry_map const& entries = default_section.get_entries();
            entry_map::const_iterator end = entries.end();
            for (entry_map::const_iterator it = entries.begin(); it != end; ++it)
            {
                std::string key = boost::to_lower_copy((*it).first);
                default_section_.insert(std::make_pair(key, (*it).second));
            }
        }

        // CLI configuration
        if (adap_ini.has_section("cli"))
        {
            saga::ini::ini cli = adap_ini.get_section("cli");

            binary_path_ = cli.get_entry("binary_path", "");
            condor_log_ = cli.get_entry("condor_log", "");
            std::string env = cli.get_entry("environment", "environment");

            if (!env.empty() && cli.has_section_full(env))
            {
                saga::ini::ini env_section = cli.get_section(env);

                cmd_launcher_.clear_environment();

                typedef saga::ini::entry_map entry_map;

                entry_map const& entries = env_section.get_entries();
                entry_map::const_iterator end = entries.end();
                for (entry_map::const_iterator it = entries.begin(); it != end; ++it)
                {
                    // To allow definition of empty environment variables,
                    // surrounding double-quotes are removed, if present in
                    // value.

                    std::string value = (*it).second;
                    std::size_t length = value.size();
                    if (length >= 2
                            && '"' == value[0]
                            && '"' == value[length - 1])
                        value = value.substr(1, length - 2);

                    cmd_launcher_.set_environment((*it).first, value);
                }
            }
        }

        initialized_ = true;
        return true;
    }

    boost::process::child job_adaptor::run_condor_command(
            std::string const & command,
            std::vector<std::string> const & arguments,
            boost::process::stream_behavior stdin_behavior,
            boost::process::stream_behavior stdout_behavior,
            boost::process::stream_behavior stderr_behavior) const
    {
        BOOST_ASSERT(initialized_);

        boost::process::command_line cl(command, "", get_binary_path());

        std::vector<std::string>::const_iterator end = arguments.end();
        for (std::vector<std::string>::const_iterator it = arguments.begin(); 
             it != end; ++it)
        {
            cl.argument(*it);
        }

        boost::process::launcher l = cmd_launcher_;
        l.set_stdin_behavior(stdin_behavior);
        l.set_stdout_behavior(stdout_behavior);
        l.set_stderr_behavior(stderr_behavior);

        if (boost::process::close_stream != stdout_behavior
                && boost::process::close_stream == stderr_behavior)
            l.set_merge_out_err(true);

        return l.start(cl);
    }

    boost::shared_ptr<shared_job_data>
    job_adaptor::find_job(std::string const & rm,
            std::string const & job_id) const
    {
        // Two-step find to reduce lock contention and avoid deadlocks with
        // registry.

        std::vector<shared_pool> candidate_pools;
        {
            pool_map::const_iterator end = pools_.end();
            for (pool_map::const_iterator it = pools_.begin(); it != end; ++it)
            {
                if ((*it).second && (*it).second->get_url() == rm)
                    candidate_pools.push_back((*it).second);
            }
        }

        boost::shared_ptr<shared_job_data> job;
        std::vector<shared_pool>::iterator end = candidate_pools.end();
        for (std::vector<shared_pool>::iterator it = candidate_pools.begin();
             it != end; ++it)
        {
            job = (*it)->get_registry()->find_job(job_id);
            if (job)
                break;
        }

        return job;
    }

}}} // namespace saga::adaptors::condor
