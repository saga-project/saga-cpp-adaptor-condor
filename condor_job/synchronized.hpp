//  Copyright (c) 2009 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_SYNCHRONIZED_HPP_INCLUDED_
#define SAGA_ADAPTORS_CONDOR_JOB_SYNCHRONIZED_HPP_INCLUDED_

#include <boost/assert.hpp>
#include <boost/version.hpp>
#include <boost/utility/addressof.hpp>
#include <boost/thread/recursive_mutex.hpp>

namespace saga { namespace adaptors { namespace condor {

    namespace detail {

        template <class Lockable>
        struct synchronized_base
            : boost::noncopyable
        {
            struct lock_base
            {
                lock_base(synchronized_base const & sb)
                    : p_(boost::addressof(sb))
                {
                #if BOOST_VERSION >= 103500
                    p_->lock_.lock();
                #else
                //  HACK: This version is specific to Boost.Thread mutexes.
                //  If need comes, locking/unlocking should be moved to an
                //  external utility and generalized...
                    boost::detail::thread::lock_ops<Lockable>
                        ::lock(p_->lock_);
                #endif
                }

                // Move constructor
                lock_base(lock_base const & that) : p_(0)
                {
                    *this = that;
                }

                ~lock_base()
                {
                    if (p_)
                    {
                #if BOOST_VERSION >= 103500
                        p_->lock_.unlock();
                #else
                //  HACK: This version is specific to Boost.Thread mutexes.
                //  If need comes, locking/unlocking should be moved to an
                //  external utility and generalized...
                        boost::detail::thread::lock_ops<Lockable>
                            ::unlock(p_->lock_);
                #endif
                    }
                }

                // Move assignment
                lock_base & operator=(lock_base const & that)
                {
                    BOOST_ASSERT(!p_
                        && "Moving to initialized lock_base not supported.");

                    std::swap(p_, that.p_);
                    return *this;
                }

                void release()
                {
                    lock_base that = *this;
                }

            protected:
                // Type is movable, hence the mutable qualification
                mutable synchronized_base const * p_;
            };

        private:
            // Also locked on const accesses
            mutable Lockable lock_;
        };

    } // namespace detail

    template <class T, class Lockable = boost::recursive_mutex>
    struct synchronized
        : detail::synchronized_base<Lockable>
    {
    private:
        typedef detail::synchronized_base<Lockable> base;

    public:
        synchronized() {}

        // TODO: More forwarding constructors...
        template <class U>
        synchronized(U const &u)
            : value_(u)
        {
        }

        // lock and const_lock are supposed to be short-lived temporaries,
        // although this is not a requirement.
        //
        // These classes have move-on-copy semantics and should be handled with
        // care, for that reason.

        struct const_lock
            : base::lock_base
        {
            typedef typename base::lock_base base_type;

            const_lock(synchronized const & sync) : base_type(sync) {}

            T const * operator->() const
            {
                return boost::addressof(get());
            }

            T const & get() const
            {
                BOOST_ASSERT(base_type::p_ && "Using uninitialized lock!");
                return static_cast<synchronized const *>(base_type::p_)->value_;
            }
        };

        struct lock
            : const_lock
        {
            lock(synchronized & sync) : const_lock(sync) {}

            T * operator->() const
            {
                return boost::addressof(get());
            }

            T & get() const
            {
                return const_cast<T &>(const_lock::get());
            }
        };

        lock operator->()
        {
            return lock(*this);
        }

        const_lock operator->() const
        {
            return const_lock(*this);
        }

    private:
        T value_;
    };

}}} // namespace saga::adaptors::condor

#endif // include guard
