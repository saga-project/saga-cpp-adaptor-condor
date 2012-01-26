//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_TEMPORARY_HPP_INCLUDED
#define SAGA_ADAPTORS_CONDOR_JOB_TEMPORARY_HPP_INCLUDED

#include <boost/config.hpp>
// Boost.Filesystem
#include <saga/saga/adaptors/utils/filesystem.hpp>

#include <memory>

#include <unistd.h>
#if defined(BOOST_WINDOWS)
#include <windows.h>
#endif

namespace saga { namespace adaptors { namespace condor {

    namespace detail {

#if defined(BOOST_WINDOWS)
        typedef HANDLE file_handle_type;
#else
        typedef int file_handle_type;
#endif

        inline bool is_handle_valid(file_handle_type fd)
        {
#if defined(BOOST_WINDOWS)
            return INVALID_HANDLE_VALUE != fd;
#else
            return 0 <= fd;
#endif
        }

    } // namespace detail

    // RAII for file descriptor returned from open_temporary_file.
    struct temporary_file
    {
        typedef detail::file_handle_type handle_type;

        temporary_file(handle_type fd, boost::filesystem::path const & path)
            : fd_(fd)
            , path_(path)
        {
        }

        ~temporary_file()
        {
            // TODO: Unlink on close should be optional and it should be
            // possible to change dynamically. This is useful, e.g., to persist
            // Condor log files while debugging.
            // Out of curiosity, is the Windows FILE_FLAG_DELETE_ON_CLOSE more
            // robust than calling unlink in the destructor on other systems?
            // For instance, in case of a hard crash where the destructor isn't
            // called, does Windows delete the file as well? What about after
            // an OS crash?  Well, we're probably not concerned with those
            // here...

            if (detail::is_handle_valid(fd_))
            {
#if defined(BOOST_WINDOWS)
                CloseHandle(fd_);
#else
                ::close(fd_);
                ::unlink(path_.string().c_str());
#endif
            }
        }

        handle_type get_fd() const
        {
            return fd_;
        }

        boost::filesystem::path get_path() const
        {
            return path_;
        }

    private:
        temporary_file(temporary_file const &);
        temporary_file & operator=(temporary_file const &);

        handle_type const fd_;
        boost::filesystem::path const path_;
    };

    // Returns the system directory for scratch space.
    boost::filesystem::path get_temporary_dir();
    std::auto_ptr<temporary_file> open_temporary_file(
            boost::filesystem::path template_ = boost::filesystem::path());

}}} // namespace saga::adaptors::condor

#endif // include guard
