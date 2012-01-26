//  Copyright (c) 2008 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../classad.hpp"

#include <boost/version.hpp>

#if BOOST_VERSION >= 103800
#include <boost/spirit/include/classic_core.hpp>
#else
#include <boost/spirit/core/parser.hpp>
#endif

#include <string>
#include <iostream>
#include <fstream>

int main(int argc, char const ** argv)
{
    using ::condor::job::classad;

    std::string data;

    // Test mode
    if (argc == 1)
    {
        static char const * alternate_argv[] = { argv[0]
                , "saga-condor-log.classad"
            };

        argc = sizeof(alternate_argv)/sizeof(alternate_argv[0]);
        argv = alternate_argv;
    }

    for (int i = 1; i < argc; ++i)
    {
        std::ifstream file(argv[i]);
        std::string line;
        while (getline(file, line))
        {
            data += line;
            data += "\n";
        }
    }

    std::cout << std::boolalpha;
    std::cout << " * Processing data:\n"
        "  ======================================"
        "======================================\n"
        << data << "@EOF\n"
        "  ======================================"
        "======================================\n\n"
        << std::flush;

    bool matched = true;
    std::size_t length = 0;
    while (matched && !data.empty())
    {
        using boost::spirit::end_p;
        using boost::spirit::parse;
        using boost::spirit::parse_info;

        classad ca;
        parse_info<std::string::iterator> pi = parse(data.begin(), data.end(),
            ca >> !end_p, classad::skipper());

        if (!pi.hit)
            break;

        length += pi.length;
        data.erase(data.begin(), pi.stop);

        std::cout << " * Matched a ClassAd entry (processed " << pi.length
            << " characters).\n"
            "ClassAd : ";

        boost::optional<classad::value> type = ca.get_attribute("MyType");
        if (type)
            std::cout << type->value_ << " ";

        std::cout << "{\n";
        for (classad::attribute_iterator iter = ca.attributes_begin(),
                end = ca.attributes_end(); iter != end; ++iter)
        {
            std::cout << "  " << iter->first << ": ("
                << iter->second.type << ") \"" << iter->second.value_ << "\"\n";
        }
        std::cout << "}\n";
    }

    std::cout << "\n *** Done parsing!\n"
        "  Full parse: " << data.empty() << "\n"
        "  Parsed:     " << length << " characters\n"
        "  Unparsed:   " << data.size() << " characters\n"
        "\n" << std::flush;

    return !data.empty();
}
