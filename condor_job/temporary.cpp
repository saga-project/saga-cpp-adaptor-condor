//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "temporary.hpp"

#include <saga/saga/detail.hpp> // safe_getenv

// Boost.Filesystem
#include <saga/saga/adaptors/utils/filesystem.hpp>
#include <boost/scoped_array.hpp>

namespace saga { namespace adaptors { namespace condor {

    // FIXME: On Windows, we're using both MAX_PATH and _MAX_PATH. Is this
    // intentional? Also, I seem to remember that there were cases where a
    // generated path could grow longer than either of those. Is there a more
    // robust way to handle path lengths there?

    boost::filesystem::path get_temporary_dir()
    {
#if defined(BOOST_WINDOWS)
        char tmpdir[MAX_PATH + 1];
        if (0 == GetTempPathA (sizeof(tmpdir), tmpdir))
            strcpy(tmpdir, "C:/tmp");
#else
        char const * tmpdir = saga::detail::safe_getenv("TMPDIR");
        // Use convention from Filesystem Hierarchy Standard
        if (!tmpdir)
            tmpdir = "/tmp";
#endif
        return tmpdir;
    }

    inline temporary_file::handle_type open_temp_file(char * tmp_name)
    {
#if defined(BOOST_WINDOWS)
        // FIXME: We're supposed to return the name of the temporary file in
        // tmp_name. Here, the name is lost into oblivion. Also, what does the
        // "tmp" do in the GetTempFileNameA call?

        // create file name
        char buffer[_MAX_PATH + 1];
        if (0 == GetTempFileNameA (tmp_name, "tmp", 0, buffer) )
            return INVALID_HANDLE_VALUE;
        return CreateFileA(buffer, GENERIC_READ|GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,
            NULL);
#else
        return mkstemp (tmp_name);
#endif
    }

    std::auto_ptr<temporary_file> open_temporary_file(
            boost::filesystem::path template_)
    {
        temporary_file::handle_type fd = temporary_file::handle_type(-1);

        {
            boost::filesystem::path dir = template_.branch_path();
            std::string leaf = boost::filesystem::filename(template_);

            // Unless a directory is specified in the template, we default to
            // using the system's temp path.
            if (!leaf.empty())
            {
                if ("/" == leaf || "." == leaf || ".." == leaf)
                {
                    if ("." != leaf || dir.empty())
                        dir = template_;
                    leaf.clear();
                }
            }

            if (dir.empty())
            {
                dir = get_temporary_dir();

                // A basic sanity check. May still fail gracefully later on.
                if (!boost::filesystem::is_directory(dir))
                    dir = boost::filesystem::path();
            }

            // Setup prefix for the temporary file, if none specified.
            if (leaf.empty())
                leaf = "temp";

#if !defined(BOOST_WINDOWS)
            // FIXME: Is windows able to provide the same semantics, i.e.,
            // provide a temporary file with a user-provided prefix? Or should
            // we instead create a directory with the prefix and generate
            // random names in there? Having a common prefix (whether in the
            // filename or as a directory helps in debugging and clean-up.

            // A random suffix is always appended to the filename.
            leaf += "-XXXXXX";
#endif

            template_ = dir / leaf;
            template_ = boost::filesystem::system_complete(template_);
        }

        boost::scoped_array<char> buffer_owner;
        char * buffer;
        {
            std::string file = template_.string();

            buffer_owner.reset(new char[file.size() + 1]);
            buffer = buffer_owner.get();

            ::strcpy(buffer, file.c_str());
        }

        std::auto_ptr<temporary_file> result;
        if (detail::is_handle_valid(fd = open_temp_file(buffer)))
        {
            // FIXME: On Windows, open_temp_file is not returning the name of
            // the temporary file in buffer. We need it here.
            template_ = buffer;
            result.reset(new temporary_file(fd, template_));
        }
        return result;
    }

}}} // namespace saga::adaptors::condor

