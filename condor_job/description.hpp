//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_DESCRIPTION_HPP_INCLUDED
#define SAGA_ADAPTORS_CONDOR_JOB_DESCRIPTION_HPP_INCLUDED

#include <saga/saga/packages/job/job_description.hpp>
#include <saga/saga/adaptors/file_transfer_spec.hpp>
#include <saga/saga/attribute.hpp>
#include <saga/impl/exception.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>


// Boost.Filesystem
#include <saga/saga/adaptors/utils/filesystem.hpp>

#include <map>
#include <string>

namespace condor { namespace job {

    // TODO: Integrate with the ClassAd thingie
    struct description
    {
        typedef std::map<std::string, std::string> attributes_type;

        description()
        {
        }

        description(attributes_type preset_attributes)
            : attributes_(preset_attributes)
        {
        }

    protected:
        template <class Ostr>
        friend Ostr & operator<<(Ostr &, description const &);

        attributes_type attributes_;
    };

    template <class Ostr>
    Ostr & operator<<(Ostr & ostr, description const & desc)
    {
        std::string content, postlog;

        typedef std::pair<std::string, std::string> attribute;

        description::attributes_type::const_iterator end = desc.attributes_.end();
        for (description::attributes_type::const_iterator it = desc.attributes_.begin();
             it != end; ++it)
        {
            if (!(*it).second.empty())
                if ((*it).first == "queue")
                    postlog     += (*it).first +  " "  + (*it).second + "\n";
                else
                    content += (*it).first + " = " + (*it).second + "\n";
        }

        ostr << (content + postlog);

        return ostr;
    }

}} // namespace condor::job

namespace saga { namespace adaptors { namespace condor { namespace detail {

    struct saga_to_condor
        : ::condor::job::description
    {
        saga_to_condor(saga::job::description const & jd,
                       saga::url const & resource_manager,
                       std::vector <saga::context> const & context_list,
                       attributes_type preset_attributes = attributes_type())
            : description(preset_attributes)
            , saga_description_(jd)
            , saga_resource_manager_(resource_manager)
            , context_list_(context_list)
        {
            using namespace saga::job::attributes;

            /*map_attribute(description_queue, "universe");
            {
                //  FIXME!
                //  supported values: Vanilla, Standard, Scheduler, Local,
                //  Grid, MPI, Java, VM
            }*/

            require_attribute   (description_executable,    "executable");
            map_attribute_vector(description_arguments,     "arguments");
            map_attribute_vector(description_environment,   "environment");

            map_attribute(description_working_directory, "remote_initialdir");

            // TODO Interactive - Not implemented
            if (saga_description_.attribute_exists(description_interactive))
            {
                std::string interactive
                    = saga_description_.get_attribute(description_interactive);

                if (!interactive.empty()
                        && boost::iequals(interactive,
                            saga::attributes::common_true))
                    SAGA_ADAPTOR_THROW_NO_CONTEXT("Condor adaptor does not "
                        "support Interactive jobs.", saga::NotImplemented);
            }

            map_attribute(description_input,    "input");
            map_attribute(description_output,   "output");
            map_attribute(description_error,    "error");

            map_attribute(description_job_start_time, "deferral_time");

            map_attribute(description_number_of_processes, "queue", "1");

            process_x509_certs();
            
            process_condorG_host();
          
            process_file_transfer();

            if (saga_description_.attribute_exists(description_job_contact))
            {
                BOOST_ASSERT(
                        saga_description_.attribute_is_vector(
                            description_job_contact)
                        && "FIXME: Reading scalar attribute as a vector!");

                std::vector<std::string> const& attribs = 
                    saga_description_.get_vector_attribute(description_job_contact);
                std::vector<std::string>::const_iterator end = attribs.end();
                for (std::vector<std::string>::const_iterator it = attribs.begin();
                     it != end; ++it)
                {
                    if (boost::starts_with((*it), "mailto:"))
                    {
                        attributes_["notify_user"] = (*it).substr(7);
                        break;
                    }
                }
            }

            // TODO Cleanup - False should be unsupported
            // unsupported_attribute(description_cleanup);

            //
            //  SPMD stuff
            //
            unsupported_attribute(description_spmd_variation);
            unsupported_attribute(description_processes_per_host);
            unsupported_attribute(description_threads_per_process);

            //
            //  Requirements
            //

            if (saga_description_.attribute_exists(description_total_cpu_time))
            {
                // Ignoring attribute
            }

            if (saga_description_.attribute_exists(description_candidate_hosts))
            {
                BOOST_ASSERT(
                        saga_description_.attribute_is_vector(
                            description_candidate_hosts)
                        && "FIXME: Reading scalar attribute as a vector!");

                std::string req;

                std::vector<std::string> const& attribs = 
                    saga_description_.get_vector_attribute(description_candidate_hosts);
                std::vector<std::string>::const_iterator end = attribs.end();
                for (std::vector<std::string>::const_iterator it = attribs.begin();
                     it != end; ++it)
                {
                    // Some adaptors are putting empty values here :-/
                    if ((*it).empty())
                        continue;

                    if (!req.empty())
                        req += " || ";
                    req += "( Machine == \"" + *it + "\" )";
                }

                if (!req.empty())
                    requirements_.push_back(req);
            }

            map_requirement(description_total_cpu_count,        "cpus");
            map_requirement(description_total_physical_memory,  "memory");

            // TODO: Do the mappings
            map_requirement(description_cpu_architecture,       "arch");
            map_requirement(description_operating_system_type,  "opsys");

            {
                std::string requirements;

                std::vector<std::string>::iterator end = requirements_.end();
                for (std::vector<std::string>::iterator it = requirements_.begin();
                     it != end; ++it)
                {
                    if (!requirements.empty())
                        requirements += " && ";
                    requirements += "(" + *it + ")";
                }

                if (!requirements.empty())
                    attributes_["requirements"] = requirements;
            }
        }

    private:
        
        std::string get_cert_info(saga::context const & c)
        {
          using namespace saga::attributes;
          std::stringstream os;
          
          os << "Context:"; 
          
          if (c.attribute_exists (saga::attributes::context_type))
            os << " type=" << c.get_attribute (context_type);
            
          if (c.attribute_exists (saga::attributes::context_type))
            os << " userproxy=" << c.get_attribute (context_userproxy);
          
          return os.str();
        }
      
        bool process_x509_certs()
        {
            using namespace saga::job::attributes;
            
            std::vector<saga::context>::const_iterator it = context_list_.begin();
            while(it != context_list_.end())
            {
              if ((*it).attribute_exists (saga::attributes::context_type) &&
                  (*it).get_attribute (saga::attributes::context_type) == "x509")
              {
                if(!(*it).attribute_exists (saga::attributes::context_userproxy)) 
                {
                  SAGA_LOG_INFO(get_cert_info(*it) + " is not usable. Userproxy attribute not set.");
                }
                else
                {
                  std::string userproxy((*it).get_attribute (saga::attributes::context_userproxy));
                  
                  if(!boost::filesystem::exists(userproxy)) 
                  {
                    SAGA_LOG_INFO(get_cert_info(*it) + " is not usable. Userproxy path doesn't exist.");
                  }
                  else 
                  {
                    SAGA_LOG_INFO(get_cert_info(*it) + " is valid. Inserting into Condor ClassAd.");
                      
                    attributes_["x509userproxy"] = userproxy;
                  }
                }
              }
              ++it;
            }
            return true;
        }
        
        bool process_condorG_host()
        {
          std::string scheme(saga_resource_manager_.get_scheme()); 
          
          if(scheme == "condorg")
          {            
            std::string url(saga_resource_manager_.get_url());
            boost::replace_first(url, "condorg://", "gt2 ");

            attributes_["universe"] = "grid";            
            attributes_["grid_resource"] = url;
            
            SAGA_LOG_INFO("Adding Condor-G host into ClassAd: " + url);
          }
          else 
          {
            attributes_["Universe"] = "vanilla";
          }      
         
          return true;
        }

      
        bool process_file_transfer()
        {
            using namespace saga::job::attributes;

            if (saga_description_.attribute_exists(description_file_transfer))
            {
                std::string input, output, output_remaps;

                std::vector<std::string> const& attribs = 
                    saga_description_.get_vector_attribute(description_file_transfer);
                std::vector<std::string>::const_iterator end = attribs.end();
                for (std::vector<std::string>::const_iterator it = attribs.begin();
                     it != end; ++it)
                {
                    std::string left, right;
                    file_transfer_operator mode;
                    if (!parse_file_transfer_specification(*it, left, mode,
                            right))
                        SAGA_ADAPTOR_THROW_NO_CONTEXT("Ill-formatted file "
                            "transfer specification: " + *it,
                            saga::BadParameter);

                    //  Appending is not natively supported.
                    if (append_local_remote == mode
                            || append_remote_local == mode)
                        SAGA_ADAPTOR_THROW_NO_CONTEXT("The Condor adaptor does "
                            "not support file append operations "
                            "(FileTransfer entry: '" + *it + "').",
                            saga::NotImplemented);

                    boost::filesystem::path l = left, r = right;
                    boost::filesystem::file_status l_status
                       = boost::filesystem::status(l);

                    switch (mode)
                    {
                    case copy_local_remote:
                        // TODO: Perform the rename locally.
                        if (saga::detail::leaf(l) != saga::detail::leaf(r))
                            SAGA_ADAPTOR_THROW_NO_CONTEXT("The Condor adaptor "
                                "does not support remote file renaming "
                                "(FileTransfer entry: '" + *it + "').",
                                saga::NotImplemented);

                        /*if (!r.root_name().empty()
                                || !r.root_directory().empty()
                                || ("./" != r.relative_path().string()
                                    && "." != r.relative_path().string()))
                            SAGA_ADAPTOR_THROW_NO_CONTEXT("The Condor adaptor "
                                "does not support placing files outside the "
                                "remote working directory "
                                "(FileTransfer entry: '" + *it + "').",
                                saga::NotImplemented); */

                        if (!exists(l_status))
                            SAGA_ADAPTOR_THROW_NO_CONTEXT("File not found on "
                                "local filesystem "
                                "(FileTransfer entry: '" + *it + "').",
                                saga::BadParameter);

                        if (is_directory(l_status))
                            SAGA_ADAPTOR_THROW_NO_CONTEXT("Directory transfer "
                                "not supported in FileTransfer attribute "
                                "(FileTransfer entry: '" + *it + "').",
                                saga::BadParameter);

                        input += ", " + left;
                        break;

                    case copy_remote_local:
                        if (!r.root_name().empty()
                                || !r.root_directory().empty()
                                || ("./" != r.relative_path().string()
                                    && "." != r.relative_path().string()))
                            SAGA_LOG_WARN(("Transferring files from outside "
                                "the remote working directory is undefined "
                                "behaviour in Condor "
                                "(FileTransfer entry: '" + *it + "').").c_str());

                        if (saga::detail::leaf(l) != saga::detail::leaf(r))
                        {
                            std::string left_ = left, right_ = right;

                            boost::replace_all(left_, "=", "\\=");
                            boost::replace_all(left_, ";", "\\;");

                            boost::replace_all(right_, "=", "\\=");
                            boost::replace_all(right_, ";", "\\;");

                            output_remaps += left_ + "=" + right_ + ";";
                        }

                        if (exists(l_status))
                            SAGA_LOG_WARN(("Destination file exists "
                                "(FileTransfer entry: '" + *it + "').").c_str());

                        output += " " + right;
                        break;

                    // Just in case...
                    default:

                        SAGA_ADAPTOR_THROW_NO_CONTEXT("Unknown FileTransfer "
                            "operator "
                            "(FileTransfer entry: '" + *it + "').",
                            saga::BadParameter);
                    }
                }

                if (!input.empty())
                    attributes_["transfer_input_files"] = input;
                if (!output.empty())
                    attributes_["transfer_output_files"] = output;
                if (!output_remaps.empty())
                    attributes_["transfer_output_remaps"]
                        = "\"" + output_remaps + "\"";

                return true;
            }

            return false;
        }

        bool map_requirement(char const * attribute,
                char const * condor_requirement)
        {
            if (saga_description_.attribute_exists(attribute))
            {
                std::string value = saga_description_.get_attribute(attribute);

                if (!value.empty())
                {
                    std::string req = condor_requirement;
                    req += " = " + saga_description_.get_attribute(attribute);

                    requirements_.push_back(req);

                    return true;
                }
            }

            return false;
        }

        void require_attribute(char const * attribute,
                char const * condor_attribute)
        {
            // TODO: Make sure it isn't empty?
            attributes_[condor_attribute]
                = saga_description_.get_attribute(attribute);
        }

        void unsupported_attribute(char const * attribute)
        {
            if (saga_description_.attribute_exists(attribute)
                    && !saga_description_.get_attribute(attribute).empty())
                SAGA_ADAPTOR_THROW_NO_CONTEXT("Condor adaptor does not support "
                    "the '" + attribute + "' attribute.", saga::NotImplemented);
        }

        bool map_attribute(char const * attribute,
                char const * condor_attribute)
        {
            // TODO: Make sure it isn't empty?
            if (saga_description_.attribute_exists(attribute))
            {
                attributes_[condor_attribute]
                    = saga_description_.get_attribute(attribute);

                return true;
            }

            return false;
        }

        bool map_attribute(char const * attribute,
                char const * condor_attribute, char const * default_value)
        {
            if (map_attribute(attribute, condor_attribute))
                return true;

            attributes_[condor_attribute] = default_value;
            return false;
        }

        //  Formats a vector of command-line arguments (or environment
        //  variables) into a string suitable for use in Condor submit
        //  descriptions.
        bool map_attribute_vector(char const * attribute,
                char const * condor_attribute)
        {
            if (saga_description_.attribute_exists(attribute))
            {
                BOOST_ASSERT(saga_description_.attribute_is_vector(attribute)
                        && "FIXME: Reading scalar attribute as a vector!");

                std::string result;

                std::vector<std::string> const& attribs = 
                    saga_description_.get_vector_attribute(attribute);
                std::vector<std::string>::const_iterator end = attribs.end();
                for (std::vector<std::string>::const_iterator it = attribs.begin();
                     it != end; ++it)
                {
                    std::string attr (*it);
                    boost::replace_all(attr, "\"", "\"\"");
                    if (boost::contains(attr, "'")
                            || boost::contains(attr, " "))
                    {
                        boost::replace_all(attr, "'", "''");
                        attr = "'" + attr + "'";
                    }
                    result += attr + " ";
                }

                if (!result.empty())
                {
                    attributes_[condor_attribute] = "\"" + result + "\"";

                    return true;
                }
            }

            return false;
        }

    private:
        saga::job::description const &     saga_description_;
        saga::url const &                saga_resource_manager_;
        std::vector<std::string>           requirements_;
        std::vector<saga::context> const & context_list_;
    };

}}}} // namespace saga::adaptors::condor::detail

#endif // include guard
