//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_CONDOR_JOB_HPP
#define SAGA_ADAPTORS_CONDOR_JOB_CONDOR_JOB_HPP

#include "condor_job_adaptor.hpp"
#include "pool_data.hpp"
#include "shared_job_data.hpp"

#include <saga/saga/adaptors/attribute.hpp>
#include <saga/impl/packages/job/job_cpi.hpp>

#include <boost/assert.hpp>
#include <boost/thread.hpp>

#include <set>
#include <string>

namespace saga { namespace adaptors { namespace condor {

    struct log_processor;

    class job_cpi_impl : public saga::adaptors::v1_0::job_cpi<job_cpi_impl>
    {
        typedef saga::adaptors::v1_0::job_cpi<job_cpi_impl> base_cpi;
        typedef boost::recursive_mutex::scoped_lock scoped_lock;

        friend struct log_processor;
        TR1::weak_ptr<saga::impl::proxy> proxy_lock_;

    public:
        job_cpi_impl(proxy * p, cpi_info const & info,
                saga::ini::ini const & glob_ini,
                saga::ini::ini const & adap_ini,
                TR1::shared_ptr<saga::adaptor> adaptor);

        ~job_cpi_impl();

        void sync_get_state(saga::job::state &);
        void sync_get_description(saga::job::description &);
        void sync_get_job_id(std::string &);

        void sync_run(saga::impl::void_t &);
        void sync_cancel(saga::impl::void_t &, double);
        void sync_wait(bool &, double);
        void sync_suspend(saga::impl::void_t &);
        void sync_resume(saga::impl::void_t &);

    private:
        std::string get_job_id()
        {
            return job_data_->full_job_id;
        }

        void set_job_id();

        bool is_state_final() const
        {
            saga::job::state state = get_state();
            return state == saga::job::Done
                || state == saga::job::Canceled
                || state == saga::job::Failed;
        }

        saga::job::state get_state() const
        {
            if (state_changed_)
            {
                shared_job_data::scoped_lock lock(job_data_->state_change_mtx);
                cached_state_ = job_data_->state;
                state_changed_ = false;
            }

            return cached_state_;
        }

        void update_state(saga::job::state state,
                std::map<std::string, std::string> const & attributes)
        {
            typedef shared_job_data::attribute_map attribute_map;

            // If the proxy no longer exists, there's no point in updating our
            // state. We should just die, already.
            TR1::shared_ptr<saga::impl::proxy> lock = this->proxy_lock_.lock();
            if (!lock)
                return;

            // Update job attributes
            attribute_map::const_iterator end = attributes.end();
            for (attribute_map::const_iterator it = attributes.begin(); 
                 it != end; ++it)
            {
                saga::adaptors::attribute job_attr(this);
                job_attr.set_attribute((*it).first, (*it).second);
            }

            // Update the state
            saga::monitorable monitor(this->proxy_);
            saga::adaptors::metric m(monitor.get_metric(
                        saga::metrics::task_state));

            if (saga::adaptors::job_state_value_to_enum(m.get_attribute(
                    saga::attributes::metric_value)) != state)
            {
                m.set_attribute(saga::attributes::metric_value,
                    saga::adaptors::job_state_enum_to_value(state));
                m.fire();
            }

            state_changed_ = true;
        }

        boost::shared_ptr<job_adaptor> get_adaptor() const
        {
            return TR1::static_pointer_cast<job_adaptor>(
                base_cpi::get_adaptor());
        }

        boost::shared_ptr<shared_job_data> job_data_;
        volatile mutable bool state_changed_;
        mutable saga::job::state cached_state_;
    };

}}} // namespace saga::adaptors::condor

#endif // include guard
