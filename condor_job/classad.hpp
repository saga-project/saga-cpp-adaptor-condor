//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SAGA_ADAPTORS_CONDOR_JOB_CLASSAD_HPP_INCLUDED
#define SAGA_ADAPTORS_CONDOR_JOB_CLASSAD_HPP_INCLUDED

#include <saga/saga/packages/job/job_description.hpp>

#include <boost/ref.hpp>
#include <boost/optional.hpp>

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>

#include <boost/version.hpp>
#if BOOST_VERSION >= 103800
#include <boost/spirit/include/classic_debug.hpp>
#include <boost/spirit/include/classic_parser_names.hpp>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/classic_utility.hpp>
#else
#include <boost/spirit/debug.hpp>
#include <boost/spirit/debug/parser_names.hpp>
#include <boost/spirit/core/non_terminal/grammar.hpp>
#include <boost/spirit/core/non_terminal/rule.hpp>
#include <boost/spirit/core/primitives/primitives.hpp>
#include <boost/spirit/core/composite/actions.hpp>
#include <boost/spirit/core/composite/directives.hpp>
#include <boost/spirit/core/composite/epsilon.hpp>
#include <boost/spirit/core/composite/kleene_star.hpp>
#include <boost/spirit/core/composite/sequence.hpp>
#include <boost/spirit/utility/chset.hpp>
#endif

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <map>
#include <string>

namespace condor { namespace job {

    //
    //  Quick notes on the parser:
    //
    //      - Lists and nested ClassAds not currently supported.
    //      - XML headers and enclosing <classads> are not parsed and have to be
    //      skipped externally.
    //
    //  TODO
    //      - Add an "unparser"
    //      - Error recovery mechanisms:
    //          * need more data
    //          * skip to next entry if malformed
    //      - Support lists and nested ClassAds
    //
    //  References:
    //      1. Marvin Solomon (2005), ClassAd XML Document Type Definition,
    //      http://www.cs.wisc.edu/condor/classad/refman/node8.html
    //
    struct classad
        : boost::spirit::grammar<classad>
    {
        template <class ForwardRange>
        bool find_and_parse(ForwardRange const & range)
        {
            typedef typename boost::range_const_iterator<ForwardRange>::type
                iterator;

            iterator begin = boost::const_begin(range);
            return find_and_parse(begin, boost::const_end(range));
        }

        template <class Iterator>
        bool find_and_parse(Iterator & first, Iterator const & last)
        {
            boost::spirit::parse_info<Iterator> pi =
                boost::spirit::parse<Iterator>(first, last, skip_to_classad());

            if (!pi.hit)
                return false;

            classad clone(*this);
            first = pi.stop;
            pi = boost::spirit::parse(first, last, clone, skipper());

            if (!pi.hit)
                return false;

            this->attributes_.swap(clone.attributes_);
            first = pi.stop;

            return true;
        }

    private:
        struct actor
        {
            struct assign
            {
                assign(std::string & s) : s_(s) {}

                template <class I>
                void operator()(I const & b, I const & e) const
                {
                    s_.assign(b, e);
                }

            private:
                std::string & s_;
            };

            template <class I>
            struct assign_iterators_
            {
                assign_iterators_(I & b, I & e) : b_(b), e_(e) {}

                void operator()(I const & b, I const & e) const
                {
                    b_ = b;
                    e_ = e;
                }

            private:
                I & b_;
                I & e_;
            };

            template <class I>
            static assign_iterators_<I> assign_iterators(I & b, I & e)
            {
                return assign_iterators_<I>(b, e);
            }

            struct clear
            {
                clear(std::string & str) : s_(str) {}

                template <class I>
                void operator()(I const &, I const &) const
                {
                    s_.clear();
                }

            private:
                std::string & s_;
            };

            struct set_attribute
            {
                set_attribute(classad const & c, std::string & k,
                        std::string const & t, std::string & v)
                    : c_(c), k_(k), t_(t), v_(v)
                {
                }

                template <class I>
                void operator()(I const &, I const &) const
                {
                    // TODO Use classad_type
                    // TODO should we process numeric character references (in
                    //      the formats &#nnn; or &#xhhh;)? That will also
                    //      require messing with encodings.

                    char const * escapes[][2] = {
                            { "&quot;", "\"" },
                            { "&amp;",  "&" },
                            { "&apos;", "'" },
                            { "&lt;",   "<" },
                            { "&gt;",   ">" }
                        };

                    boost::to_lower(k_);

                    for (std::size_t i = 0;
                            i < sizeof(escapes)/sizeof(*escapes); ++i)
                    {
                        boost::replace_all(k_, escapes[i][0], escapes[i][1]);
                        boost::replace_all(v_, escapes[i][0], escapes[i][1]);
                    }

                    value v = { t_, v_ };
                    c_.attributes_[ k_ ] = v;
                }

            private:
                classad const & c_;
                std::string & k_;
                std::string const & t_;
                std::string & v_;
            };
        };

    public:
        enum classad_type
        {
            Integer,
            Real,
            String,
            ClassAdExpression,
            Boolean,
            AbsoluteTime,
            RelativeTime,
            Undefined,
            Error,
            List
        };

        struct value
        {
            // classad_type type;
            std::string type;
            std::string value_;
        };

        typedef std::map<std::string, value> attribute_map_type;
        typedef attribute_map_type::iterator attribute_iterator;

        void clear()
        {
            attributes_.clear();
        }

        void set_attribute(std::string key, /* classad_type type, */
                std::string const & type,
                std::string const & val)
        {
            boost::to_lower(key);

            value v = { type, val };
            attributes_[ key ] = v;
        }

        boost::optional<value> get_attribute(std::string key) const
        {
            boost::to_lower(key);

            boost::optional<value> attr;
            attribute_iterator iter = attributes_.find(key);

            if (attributes_.end() != iter)
                attr.reset(iter->second);
            return attr;
        }

        void remove_attribute(std::string const & key) const
        {
            attributes_.erase(key);
        }

        bool has_attribute(std::string const & key) const
        {
            return attributes_.count(key) ? true : false;
        }

        attribute_iterator attributes_begin()
        {
            return attributes_.begin();
        }

        attribute_iterator attributes_end()
        {
            return attributes_.end();
        }

        typedef boost::spirit::space_parser skipper;

        struct skip_to_classad
            : boost::spirit::grammar<skip_to_classad>
        {
            template <class Scanner>
            struct definition
            {
                typedef boost::spirit::rule<Scanner> rule;

                definition(skip_to_classad const &)
                {
                    BOOST_SPIRIT_DEBUG_RULE(skip_to_classad_);

                    using boost::spirit::alnum_p;
                    using boost::spirit::anychar_p;
                    using boost::spirit::eps_p;
                    using boost::spirit::str_p;

                    skip_to_classad_ =
                        *(anychar_p - (str_p("<c") >> (eps_p - alnum_p)));
                }

                rule const & start() const
                {
                    return skip_to_classad_;
                }

                rule skip_to_classad_;
            };
        };

        template <class Scanner>
        struct definition
        {
            typedef boost::spirit::rule<Scanner> rule;

            definition(classad const & self)
            {
                BOOST_SPIRIT_DEBUG_RULE(classad_);
                BOOST_SPIRIT_DEBUG_RULE(attribute_);
                BOOST_SPIRIT_DEBUG_RULE(type_);
                BOOST_SPIRIT_DEBUG_RULE(bool_value_);
                BOOST_SPIRIT_DEBUG_RULE(value_);

                using boost::spirit::ch_p;
                using boost::spirit::chset;
                using boost::spirit::eps_p;
                using boost::spirit::lexeme_d;
                using boost::spirit::str_p;

                classad_ =
                    str_p("<c>") >> *( attribute_ ) >> "</c>";

                attribute_ =
                    str_p("<a") >> "n=\"" >> ( * ~ch_p('\"') )
                /* >>>>>>>>>>>>>>>>>>>>> */ [ actor::assign(attr_key_) ]
                    >> '\"' >> '>' >> value_ >> str_p("</a>")
                /* >>>>>>>>>>>>>>>>>>>>> */ [ actor::set_attribute(self,
                                                    attr_key_, attr_type_,
                                                    attr_value_) ];

                bool_value_ =
                    lexeme_d [ ch_p('<') >> str_p("b")
                /* >>>>>>>>>>>>>>>>>>>>> */ [ actor::assign(attr_type_) ] ]
                    >> "v=\"" >> ( * ~ch_p('\"') )
                /* >>>>>>>>>>>>>>>>>>>>> */ [ actor::assign(attr_value_) ]
                    >> ch_p('\"') >> str_p("/>");

                value_ =
                    bool_value_ | lexeme_d [ ch_p('<') >>
                    // TODO: lists ('l') and nested classads ('c')
                    (chset<char>("eirs")| "at" | "rt" | "un" | "er")
                /* >>>>>>>>>>>>>>>>>>>>> */ [ actor::assign(attr_type_) ]
                /* >>>>>>>>>>>>>>>>>>>>> */ [ actor::assign_iterators(
                                                    type_begin_, type_end_) ] ]
                    >> (ch_p('>')
                        >> ( * ~ch_p('<') )
                /* >>>>>>>>>>>>>>>>>>>>> */ [ actor::assign(attr_value_) ]
                        >> lexeme_d [ "</" >> str_p(boost::ref(type_begin_),
                            boost::ref(type_end_)) ] >> '>'
                        | str_p("/>")
                /* >>>>>>>>>>>>>>>>>>>>> */ [ actor::clear(attr_value_) ]);
            }

            rule const & start() const
            {
                return classad_;
            }

        private:
            std::string attr_key_;
            std::string attr_type_;
            std::string attr_value_;

            typedef typename Scanner::iterator_t iterator;
            iterator type_begin_;
            iterator type_end_;

            rule classad_, attribute_, type_, bool_value_, value_;
        };

    private:
        mutable std::map<std::string, value> attributes_;
    };

}} // namespace condor::job

namespace saga { namespace adaptors { namespace condor { namespace detail {

    struct condor_to_saga
        : saga::job::description
    {
        condor_to_saga(::condor::job::classad const & ca)
            : classad_(ca)
        {
            using namespace saga::job::attributes;

            map_attribute("Universe", description_queue);

            map_attribute("Executable",         description_executable);
            //  map_attribute_vector("Arguments",   description_arguments);
            //  map_attribute_vector("Environment", description_environment);

            map_attribute("Remote_InitialDir",  description_working_directory);

            //  description_interactive

            map_attribute("Input",  description_input);
            map_attribute("Output", description_output);
            map_attribute("Error",  description_error);

            map_attribute("Deferral_Time", description_job_start_time);

            //  map_attribute(description_number_of_processes, "Queue", "1");

            //  description_file_transfer

            {
                boost::optional< ::condor::job::classad::value> attr
                    = classad_.get_attribute("Notify_User");

                if (attr && !attr->value_.empty())
                {
                    std::vector<std::string> contacts;
                    contacts.push_back("mailto:" + attr->value_);

                    this->set_vector_attribute(
                        description_job_contact, contacts);
                }
            }

            //  description_cleanup

            //
            //  SPMD stuff
            //
            //  description_spmd_variation
            //  description_processes_per_host
            //  description_threads_per_process

            //
            //  Requirements
            //

            //  description_total_cpu_time
            //  description_candidate_hosts

            //  map_requirement("Cpus",     description_total_cpu_count);
            //  map_requirement("Memory",   description_total_physical_memory);

            //  map_requirement("Arch",     description_cpu_architecture);
            //  map_requirement("OpSys",    description_operating_system_type);
        }

        bool map_attribute(char const * attribute, char const * saga_attribute)
        {
            boost::optional< ::condor::job::classad::value> attr
                = classad_.get_attribute(attribute);

            if (attr)
            {
                this->set_attribute(saga_attribute, attr->value_);
                return true;
            }

            return false;
        }

        //  bool map_attribute_vector(char const * attribute,
        //          char const * condor_attribute)
        //  {
        //      return false;
        //  }

    private:
        ::condor::job::classad const & classad_;
    };

}}}} // namespace saga::adaptors::condor::detail

#endif // include guard
