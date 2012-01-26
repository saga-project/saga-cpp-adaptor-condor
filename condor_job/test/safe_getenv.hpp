//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_TEST_SAFE_GETENV_MOCKUP_ACTIVATED
#define SAGA_ADAPTORS_CONDOR_JOB_TEST_SAFE_GETENV_MOCKUP_ACTIVATED

#include <stdlib.h>

namespace saga {

  namespace detail {

    // Not really safe.
    //
    // This is only useful to avoid linking the saga libs in the temporary
    // tests.
    char * safe_getenv(char const * name)
    {
        return ::getenv(name);
    }

  }

} // saga

#endif // include guard
