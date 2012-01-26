//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_HELPER_HPP_INCLUDED
#define SAGA_ADAPTORS_CONDOR_JOB_HELPER_HPP_INCLUDED

#include <saga/saga/url.hpp>
#include <saga/saga/packages/job/job_description.hpp>
#include <saga/saga/adaptors/file_transfer_spec.hpp>
#include <saga/impl/exception.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/regex.hpp>

namespace saga { namespace adaptors { namespace condor { namespace detail {

    struct helper
    {
        //  Parses a job_id in the format '[backend URL]-[id]' into the
        //  corresponding parts.
        //
        //  NOTE: Throws on parsing failure. Could simply return a boolean value
        //  indicating success, but saga::url already throws on a bad URL.
        static void parse_jobid(std::string const & jobid, saga::url & backend,
                std::string & id)
        {
            static const boost::regex re("\\[(.+)\\]-\\[(.+)\\]");

            boost::smatch match;
            if (!boost::regex_match(jobid, match, re))
                SAGA_THROW_NO_OBJECT("Invalid job ID: " + jobid + " (expected "
                        "format: '[' <Backend URL> ']-[' <ID> ']').",
                        saga::BadParameter);

            // saga::url validates the URL to be minimally valid. We enforce
            // some further constraints.
            saga::url backend_ = match.str(1);
            if (backend_.get_scheme().empty())
                SAGA_THROW_NO_OBJECT("Backend URL has no scheme set.",
                        saga::BadParameter);
            if (backend_.get_host().empty())
                SAGA_THROW_NO_OBJECT("Backend URL has no host set.",
                        saga::BadParameter);

            // Everything looks good, forward the results.
            backend = backend_;
            id = match.str(2);
        }
    };

}}}} // namespace saga::adaptors::condor::detail

#endif // include guard
