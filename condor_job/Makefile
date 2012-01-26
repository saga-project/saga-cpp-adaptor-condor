#  Copyright (c) 2005-2006 Andre Merzky (andre@merzky.net)
#
#  Use, modification and distribution is subject to the Boost Software
#  License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt)

SAGA_SUBDIRS      = test

SAGA_SRC          = $(wildcard *.cpp)
SAGA_OBJ          = $(SAGA_SRC:%.cpp=%.o)

SAGA_ADAPTOR_NAME = condor_job
SAGA_ADAPTOR_INI  = condor_job_adaptor.ini
SAGA_ADAPTOR_OBJ  = $(SAGA_OBJ)

SAGA_ADAPTOR_PACKAGES = job

-include ../config/make.cfg
-include $(SAGA_MAKE_INCLUDE_ROOT)/saga.adaptor.mk

distclean::
	@$(RM) condor_job_adaptor.ini
