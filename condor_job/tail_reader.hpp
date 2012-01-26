//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_TAIL_READER_HPP_INCLUDED
#define SAGA_ADAPTORS_CONDOR_JOB_TAIL_READER_HPP_INCLUDED

#include <boost/assert.hpp>
#include <boost/iostreams/concepts.hpp>

#include <string>

#include <fcntl.h>
#include <unistd.h>

#if defined(BOOST_WINDOWS)
// we define this for now to make everything compile, this needs to be fixed, 
// though, as the tail_reader is currently non-functional.
#define O_NONBLOCK 0
#endif

namespace saga { namespace adaptors { namespace condor { namespace detail {

    // Non-blocking file device, for which EOF is never signalled. This allows
    // a file to be read as it is being written, much like calling "tail(1) -f".
    // tail_reader is a model of the InputSeekableDevice from the Boost
    // Iostreams library.
    struct tail_reader
        : boost::iostreams::device<boost::iostreams::input_seekable>
    {
        tail_reader()
            : fd_(-1)
        {
        }

        tail_reader(std::string const & filename)
            : fd_(-1), filename_(filename)
        {
        }

        ~tail_reader()
        {
            if (is_open())
                ::close(fd_);
        }

        // Open current file
        bool open()
        {
            BOOST_ASSERT(!is_open() && "File already open.");
            return 0 <= (fd_ = ::open(filename_.c_str(), O_RDONLY | O_NONBLOCK
                        | O_NOCTTY));
        }

        // Close current file descriptor, if any.
        void close()
        {
            if (is_open())
                ::close(fd_);
            fd_ = -1;
        }

        // Closes current file descriptor, if any. Prepares file identified by
        // filename for "tailing".
        void reset(std::string const & filename)
        {
            close();
            filename_ = filename;
        }

        // Do we have an open file descriptor?
        bool is_open() const
        {
            return !(fd_ < 0);
        }

        // Current filename
        std::string get_filename() const
        {
            return filename_;
        }

        ////////////////////////////////////////////////////////////////////
        //
        // InputSeekableDevice interface
        //

        std::streamsize read(char * buf, std::streamsize n)
        {
            std::streamsize count = 0;
            if (n && (is_open() || open())
                    && (0 <= (count = ::read(fd_, buf, n))))
                return count;

            return 0;
        }

        std::streampos seek(boost::iostreams::stream_offset off,
                std::ios_base::seekdir way)
        {
            if (!is_open() && !open())
                return -1;

            int whence;
            switch (way)
            {
                case std::ios_base::beg:
                    whence = SEEK_SET;
                    break;
                case std::ios_base::cur:
                    whence = SEEK_CUR;
                    break;
                case std::ios_base::end:
                    whence = SEEK_END;
                    break;

                // This fixes a warning with g++ 4.2
                default:
                    BOOST_ASSERT(false
                        && "Unsupported seek direction requested.");
            }

            return ::lseek(fd_, (long)off, whence);
        }

    private:
        // Non-copyable
        tail_reader(tail_reader const &);
        tail_reader & operator=(tail_reader const &);

        int fd_;
        std::string filename_;
    };

}}}} // namespace saga::adaptors::condor::detail

#endif // include guard
