//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_STATUS_HPP
#define SAGA_ADAPTORS_CONDOR_JOB_STATUS_HPP

#include <saga/saga/packages/job/job.hpp>

namespace condor { namespace job {

    struct status
    {
        enum JobStatus {
            unknown          = -1,  // Internal use. This should never be
                                    // returned by Condor

            unexpanded       = 0,   // According to the Condor manual this state
                                    // was only used by older versions of Condor
            idle             = 1,
            running          = 2,
            removed          = 3,
            completed        = 4,
            held             = 5,
            submission_error = 6
        };

        status(JobStatus st)
            : status_(st)
        {
            if (status_ < 0 || status_ > 6)
                status_ = unknown;
        }

        status(std::string str)
            : status_(JobStatus(boost::lexical_cast<int>(str)))
        {
            if (status_ < 0 || status_ > 6)
                status_ = unknown;
        }

        JobStatus get_status() const
        {
            return status_;
        }

        operator saga::job::state() const
        {
            switch (status_)
            {
            case unexpanded:
            case idle:
            case running:
                return saga::job::Running;

            case removed:
                return saga::job::Canceled;

            case completed:
                return saga::job::Done;

            case held:
                return saga::job::Suspended;

            case submission_error:
                return saga::job::Failed;

            default:
                return saga::job::Unknown;
            }
        }

        std::string get_string() const
        {
            return strings[status_ + 1];
        }

        char get_abbreviation() const
        {
            return abbreviations[status_ + 1];
        }

    private:
        JobStatus status_;

        static char const * const strings [];
        static char const abbreviations [];
    };

    bool operator==(status const & lhs, status const & rhs)
    {
        return lhs.get_status() == rhs.get_status();
    }

    char const * const status::strings [] = {
            "Unknown", // Not a condor status

            "Unexpanded",
            "Idle",
            "Running",
            "Removed",
            "Completed",
            "Held",
            "Submission Error"
        };

    char const status::abbreviations [] = {
            '?', // Not a condor status

            'U',
            'I',
            'R',
            'X',
            'C',
            'H',
            'E',

            '\0' // Just in case...
        };

}} // namespace condor::job

#endif // include guard
