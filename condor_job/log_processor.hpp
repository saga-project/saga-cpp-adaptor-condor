//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_LOG_PROCESSOR_HPP
#define SAGA_ADAPTORS_CONDOR_JOB_LOG_PROCESSOR_HPP

#include "classad.hpp"
#include "condor_job.hpp"
#include "tail_reader.hpp"

#include <saga/saga-defs.hpp>
#include <saga/saga/packages/job/job.hpp>

#include <boost/version.hpp>
#if BOOST_VERSION >= 103800
#include <boost/spirit/include/classic_parser.hpp>
#else
#include <boost/spirit/core/parser.hpp>
#endif
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/version.hpp>

#include <deque>
#include <string>

namespace saga { namespace adaptors { namespace condor {

    struct log_processor
    {
        // avoid warning about using this in constructor initializer list
        log_processor* This() { return this; }

        log_processor(std::string const & filename,
                synchronized<job_registry> & registry)
            : filename_(filename)
            , registry_(registry)
        #if BOOST_VERSION < 103500
            , interrupt_thread_(false)
        #endif
            , log_initialized_(false)
            , thread_(boost::bind<void>(boost::ref(*This())))
        {
            SAGA_LOG_DEBUG(("Condor adaptor: Processing log "
                + filename_).c_str());

            boost::mutex::scoped_lock lock(log_initialized_mtx_);
            while (!log_initialized_)
                log_initialized_cond_.wait(lock);
        }

        ~log_processor()
        {
            SAGA_LOG_DEBUG(("Condor adaptor: Closing log "
                + filename_).c_str());

        #if BOOST_VERSION >= 103500
            thread_.interrupt();
        #else
            interrupt_thread_ = true;
        #endif

            thread_.join();
        }

        void process_log_entry(::condor::job::classad & c);

        void operator()()
        {
            detail::tail_reader log(filename_);
            log.seek(0, std::ios_base::end);

            {
                boost::mutex::scoped_lock lock(log_initialized_mtx_);
                log_initialized_ = true;
            }
            log_initialized_cond_.notify_one();

            std::deque<char> data;
            boost::xtime t;

            // When holding on to an incomplete record we don't want to allow
            // the record to delay processing of other records in the queue
            // indefinitely.
            int waited_for_retry = 0;

            // Number of seconds to wait for a full entry.
            static const int max_wait_for_retry = 5;

        #if BOOST_VERSION >= 103500
            while (!boost::this_thread::interruption_requested())
        #else
            while (!interrupt_thread_)
        #endif
            {
                boost::xtime_get(&t, boost::TIME_UTC);
                {
                    char buffer[1024];
                    std::streamsize n;

                    // Can't use select, poll and such because they return
                    // immediately when reading from regular files. Resorting to
                    // active wait.
                    while (0 >= (n = log.read(buffer, sizeof(buffer))))
                    {
                    #if BOOST_VERSION < 103500
                        if (interrupt_thread_)
                            return;
                    #endif

                        ++t.sec;
                        boost::thread::sleep(t);

                        if (waited_for_retry)
                            if (++waited_for_retry > max_wait_for_retry)
                            {
                                n = 0;

                                // Go try what we have
                                break;
                            }
                    }

                    data.insert(data.end(), buffer, buffer + n);
                }

                while (!data.empty())
                {
                    if (waited_for_retry > max_wait_for_retry)
                    {
                        SAGA_LOG_WARN("Condor adaptor (log processor): "
                            "Skipping incomplete log entry.");

                        waited_for_retry = 0;
                        data.pop_front();
                    }

                    ::condor::job::classad c;
                    std::deque<char>::iterator iter = data.begin();

                    bool hit = c.find_and_parse(iter, data.end());
                    data.erase(data.begin(), iter);

                    if (hit)
                    {
                        waited_for_retry = 0;
                        this->process_log_entry(c);
                    }
                    else if (data.empty())
                        waited_for_retry = 0;
                    else
                    {
                        // Incomplete ClassAd entry, go get more input
                        break;
                    }
                }
            }
        }

    private:
        std::string filename_;
        synchronized<job_registry> & registry_;

    #if BOOST_VERSION < 103500
        volatile bool interrupt_thread_;
    #endif

        volatile bool log_initialized_;
        boost::mutex log_initialized_mtx_;
        boost::condition log_initialized_cond_;

        boost::thread thread_;
    };

}}} // namespace saga::adaptors::condor

#endif // include guard
