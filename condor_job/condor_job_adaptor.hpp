//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_CONDOR_JOB_ADAPTOR_HPP
#define SAGA_ADAPTORS_CONDOR_JOB_CONDOR_JOB_ADAPTOR_HPP

#include "helper.hpp"
#include "pool_data.hpp"
#include "shared_job_data.hpp"

#include <saga/saga/adaptors/adaptor.hpp>
#include <saga/saga/adaptors/utils/is_local_address.hpp>

#include <boost/assert.hpp>
#include <boost/process.hpp>

#include <map>
#include <memory>

namespace saga { namespace adaptors { namespace condor {

    class job_adaptor : public saga::adaptor
    {
        typedef saga::impl::v1_0::preference_type preference_type;

        typedef boost::shared_ptr<pool> shared_pool;

        saga::impl::adaptor_selector::adaptor_info_list_type
            adaptor_register(saga::impl::session * s);

        bool init(saga::impl::session *, saga::ini::ini const& glob_ini,
            saga::ini::ini const& adap_ini);

        std::string get_name(void) const
        {
            return BOOST_PP_STRINGIZE(SAGA_ADAPTOR_NAME);
        }

        struct scoped_lock
        {
            scoped_lock(job_adaptor & adaptor)
                : adaptor_(adaptor)
            {
                adaptor_.lock_data();
            }

            ~scoped_lock()
            {
                adaptor_.unlock_data();
            }

        private:
            job_adaptor & adaptor_;
        };

    public:
        job_adaptor()
            : initialized_(false)
        {
        }

        std::string get_binary_path() const
        {
            BOOST_ASSERT(initialized_);
            return binary_path_;
        }

        std::map<std::string, std::string> const &
        get_default_job_attributes() const
        {
            BOOST_ASSERT(initialized_);
            return default_section_;
        }

        boost::process::child run_condor_command(std::string const & command,
            std::vector<std::string> const & arguments
                = std::vector<std::string>(),
            boost::process::stream_behavior stdin_behavior
                = boost::process::redirect_stream,
            boost::process::stream_behavior stdout_behavior
                = boost::process::redirect_stream,
            boost::process::stream_behavior stderr_behavior
                = boost::process::close_stream) const;

        std::string validate_rm(saga::url url) const
        {
            if (url.get_string().empty() && initialized_)
                return default_rm_;

            /*if (!url.get_userinfo().empty()
                    || -1 != url.get_port()
                    || !(url.get_path().empty() || "/" == url.get_path())
                    || !url.get_query().empty()
                    || !url.get_fragment().empty())
                SAGA_ADAPTOR_THROW_NO_CONTEXT("Information overload. Don't "
                    "know what to do with user, port, path, query or "
                    "fragment information provided in URL: "
                    + url.get_string() + ".", saga::BadParameter);*/

            std::string const & scheme = url.get_scheme();
            //if (scheme.empty() || scheme == "any")
            //    url.set_scheme("condor");

            if (scheme == "condor")
            {
              if (url.get_host().empty())
                url.set_host("localhost");
                
              else if (!saga::adaptors::utils::is_local_address(url))
                SAGA_ADAPTOR_THROW_NO_CONTEXT("Job submission to remote "
                  "pools not implemented. Try condorg://HOSTNAME instead if you want to submit to a Condor-G resource.", 
                  saga::BadParameter);
            }
            else if (scheme == "condorg")
            {
              if (url.get_host().empty())
                SAGA_ADAPTOR_THROW_NO_CONTEXT("Condor-G (condorg://) requires a hostname.",
                saga::BadParameter);
            }
            else
            {
              SAGA_ADAPTOR_THROW_NO_CONTEXT("Adaptor supports 'condor' "
                "and 'condorg' URL schemes, '" + scheme + "' is not supported.",
                saga::BadParameter);
            }

            return url.get_string();
        }

        // We map URLs to pool_data for the jobs we start. When we pick up
        // running jobs, we map using the Log filename, instead.
        shared_pool get_pool(std::string rm)
        {
            rm = validate_rm(rm);

            scoped_lock lck(*this);

            shared_pool & sp = pools_[rm];
            if (!sp)
                sp.reset(new pool(rm, condor_log_));
            return sp;
        }

        shared_pool get_pool(std::string rm, std::string log)
        {
            rm = validate_rm(rm);

            scoped_lock lck(*this);

            shared_pool & sp = pools_[log];
            if (!sp)
                sp.reset(new pool(rm, log));
            return sp;
        }

        boost::shared_ptr<shared_job_data>
        find_job(std::string const & rm, std::string const & job_id) const;

    private:
        volatile bool initialized_; // controls access to immutable data

        // --- Begin Immutable data --- //
        std::string default_rm_;
        std::string binary_path_;
        std::string condor_log_;
        std::map<std::string, std::string> default_section_;

        boost::process::launcher cmd_launcher_;
        // --- End Immutable data --- //

        // --- Access controlled by CPI mutex --- //
        typedef std::map<std::string, shared_pool> pool_map;
        pool_map pools_;
    };

}}} // namespace saga::adaptors::condor

#endif // include guard
