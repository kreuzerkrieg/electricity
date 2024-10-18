#include <boost/date_time.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>


namespace bpo = boost::program_options;
namespace fs = std::filesystem;

static boost::gregorian::date parse_date(const std::string& date) {
    std::vector<std::string> data;
    boost::algorithm::split(data, date, boost::is_any_of("/"));
    if (data.size() != 3) {
        throw std::invalid_argument("Wrong date format");
    }
    return boost::gregorian::date(
        std::strtoul(data[2].c_str(), nullptr, 10), std::strtoul(data[1].c_str(), nullptr, 10), std::strtoul(data[0].c_str(), nullptr, 10));
}

static boost::posix_time::time_duration parse_time(const std::string& date) {
    std::vector<std::string> data;
    boost::algorithm::split(data, date, boost::is_any_of(":"));
    if (data.size() < 2) {
        throw std::invalid_argument("Wrong time format");
    }
    return boost::posix_time::time_duration(
        std::strtol(data[0].c_str(), nullptr, 10), std::strtol(data[1].c_str(), nullptr, 10), data.size() > 2 ? std::strtol(data[2].c_str(), nullptr, 10) : 0);
}

int main(int argc, char* argv[]) {
    constexpr double kWh_price = .5252;
    bpo::options_description desc("Available options");
    desc.add_options()("help,h", "produce help message")(
        "input-file,f", bpo::value<std::string>()->required()->value_name("meter_22016209_LP_17-10-2024.csv"), "input stats file")(
        "from", bpo::value<std::string>()->value_name("20240801T000000"), "from date and/or time, ISO format (optional)")(
        "to", bpo::value<std::string>()->value_name("20240901T000000"), "to date and/or time, ISO format (optional)")(
        "time-range, r", bpo::value<std::string>()->value_name("7:00-17:00"), "time range (optional)");

    bpo::variables_map vm;
    try {
        bpo::store(bpo::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        bpo::notify(vm);
    } catch (const bpo::error& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        std::cout << desc << std::endl;
        return 1;
    }
    bpo::notify(vm);

    std::string input_file;
    if (vm.contains("input-file")) {
        input_file = vm["input-file"].as<std::string>();
    } else {
        std::cerr << "Error: input file not specified" << std::endl;
        return 1;
    }

    boost::posix_time::ptime from_date = boost::posix_time::min_date_time;
    if (vm.contains("from")) {
        from_date = boost::posix_time::from_iso_string(vm["from"].as<std::string>());
    }

    boost::posix_time::ptime to_date = boost::posix_time::max_date_time;
    if (vm.contains("to")) {
        from_date = boost::posix_time::from_iso_string(vm["to"].as<std::string>());
    }

    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file " << input_file << std::endl;
        return 1;
    }

    std::string line;
    std::map<boost::posix_time::time_duration, double> time_map;
    std::vector<std::string> data(2);
    auto unquote = [](const std::string& s) {
        if (s.front() == '\"' && s.back() == '\"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    };
    while (std::getline(file, line)) {
        data.clear();
        boost::algorithm::split(data, line, boost::is_any_of(","));
        if (data.size() != 3) {
            std::cerr << "Invalid string \"" << line << "\"" << std::endl;
            continue;
        }

        try {
            boost::posix_time::ptime row_datetime = boost::posix_time::ptime(parse_date(unquote(data[0])), parse_time(unquote(data[1])));
            if (row_datetime > from_date && row_datetime < to_date) {
                time_map[row_datetime.time_of_day()] += std::strtod(data[2].c_str(), nullptr);
            }
        } catch (...) {
            std::cerr << "Invalid string \"" << line << "\"" << std::endl;
            continue;
        }
    }

    file.close();

    if (vm.contains("time-range")) {
        std::vector<std::string> time_points;
        boost::algorithm::split(time_points, vm["time-range"].as<std::string>(), boost::is_any_of("-"));
        if (time_points.size() != 2) {
            throw std::invalid_argument("Wrong time range format");
        }
        std::tuple<boost::posix_time::time_duration, boost::posix_time::time_duration> time_range{parse_time(time_points[0]), parse_time(time_points[1])};
        if (std::get<0>(time_range) > std::get<1>(time_range)) {
            throw std::invalid_argument("Wrong time range format");
        }
        std::tuple kwhs{.0, 0.};
        for (const auto& [time, kw] : time_map) {
            if (time >= std::get<0>(time_range) && time <= std::get<1>(time_range)) {
                std::get<0>(kwhs) += kw;
            } else {
                std::get<1>(kwhs) += kw;
            }
        }
        std::cout << std::setw(2) << std::setfill('0') << std::get<0>(time_range).hours() << ":" << std::setw(2) << std::setfill('0')
                  << std::get<0>(time_range).minutes() << "-" << std::setw(2) << std::setfill('0') << std::get<1>(time_range).hours() << ":" << std::setw(2)
                  << std::setfill('0') << std::get<1>(time_range).minutes() << "\t" << std::get<0>(kwhs) << "kWh"
                  << ", " << std::get<0>(kwhs) * kWh_price << "NIS" << std::endl;
        std::cout << "The rest:\t" << std::get<1>(kwhs) << "kWh"
                  << ", " << std::get<1>(kwhs) * kWh_price << "NIS" << std::endl;
    } else {
        auto output_plot = fs::path(input_file).replace_extension(".html");
        std::ofstream htmlFile(output_plot);
        if (!htmlFile.is_open()) {
            std::cerr << "Failed to open the plot output file " << input_file << std::endl;
            return 1;
        }
        boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
        htmlFile << R"(
<!doctype html>
<html>
<head>
    <meta charset="utf-8">
    <title>kW consumption</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <link rel="stylesheet" href="uPlot/dist/uPlot.min.css">
</head>
<body>
<script src="uPlot/dist/uPlot.iife.min.js"></script>
<script>
    let data = [)"
                 << std::endl;
        std::string x_axis = "[";
        std::string y_axis = "[";
        for (const auto& [time, kw] : time_map) {
            std::cout << std::setw(2) << std::setfill('0') << time.hours() << ":" << std::setw(2) << std::setfill('0') << time.minutes()
                      << /*"\t" << std::setw(kw) << std::setfill('*') << "" <<*/ "\t" << kw << /*"KWh" <<*/ std::endl;
            x_axis += std::to_string(time.total_seconds()) + ", ";
            y_axis += std::to_string(kw) + ", ";
        }
        x_axis += "],";
        y_axis += "],";
        htmlFile << x_axis << std::endl;
        htmlFile << y_axis << std::endl;

        htmlFile << R"a(
];
    let opts = {
        title: "kW consumption",
        id: "chart1",
        class: "my-chart",
        width: 1900,
        height: 600,
        series: [
            {},
            {
                // initial toggled state (optional)
                show: true,

                spanGaps: false,

                // in-legend display
                label: "kWh",
                // value: (self, rawValue) => "$" + rawValue.toFixed(2),

                // series style
                stroke: "red",
                width: 1,
                fill: "rgba(255, 0, 0, 0.3)",
        dash: [10, 5],
    }
    ],
};

let uplot = new uPlot(opts, data, document.body);
</script>
</body>
</html>)a";
        htmlFile.close();
    }
    return 0;
}
