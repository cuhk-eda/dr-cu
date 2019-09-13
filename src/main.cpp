#include "db/Database.h"
#include "global.h"
#include "multi_net/Router.h"

// -----------------------------------------------------------------------------

void signalHandler(int signum) {
    std::cout << "Signal (" << signum << ") received. Exiting...\n";

    // cleanup and close up stuff here

    std::exit(signum);
}

// -----------------------------------------------------------------------------

void runISPD18Flow(const boost::program_options::variables_map& vm) {
    db::setting.makeItSilent();

    Rsyn::Session session;

    // Parse options
    // required
    std::string lefFile = vm.at("lef").as<std::string>();
    std::string defFile = vm.at("def").as<std::string>();
    std::string guideFile = vm.at("guide").as<std::string>();
    db::setting.numThreads = vm.at("threads").as<int>();
    db::setting.tat = vm.at("tat").as<int>();
    db::setting.outputFile = vm.at("output").as<std::string>();
    // optional
    // multi_net
    if (vm.count("multiNetVerbose")) {
        db::setting.multiNetVerbose =
            db::VerboseLevelT::_from_string(vm.at("multiNetVerbose").as<std::string>().c_str());
    }
    if (vm.count("multiNetScheduleSortAll")) {
        db::setting.multiNetScheduleSortAll = vm.at("multiNetScheduleSortAll").as<bool>();
    }
    if (vm.count("multiNetScheduleReverse")) {
        db::setting.multiNetScheduleReverse = vm.at("multiNetScheduleReverse").as<bool>();
    }
    if (vm.count("multiNetScheduleSort")) {
        db::setting.multiNetScheduleSort = vm.at("multiNetScheduleSort").as<bool>();
    }
    if (vm.count("rrrIters")) {
        db::setting.rrrIterLimit = vm.at("rrrIters").as<int>();
    }
    if (vm.count("rrrWriteEachIter")) {
        db::setting.rrrWriteEachIter = vm.at("rrrWriteEachIter").as<bool>();
    }
    if (vm.count("rrrInitVioCostDiscount")) {
        db::setting.rrrInitVioCostDiscount = vm.at("rrrInitVioCostDiscount").as<double>();
    }
    if (vm.count("rrrFadeCoeff")) {
        db::setting.rrrFadeCoeff = vm.at("rrrFadeCoeff").as<double>();
    }
    // single_net
    if (vm.count("defaultGuideExpand")) {
        db::setting.defaultGuideExpand = vm.at("defaultGuideExpand").as<int>();
    }
    if (vm.count("diffLayerGuideVioThres")) {
        db::setting.diffLayerGuideVioThres = vm.at("diffLayerGuideVioThres").as<int>();
    }
    if (vm.count("wrongWayPointDensity")) {
        db::setting.wrongWayPointDensity = vm.at("wrongWayPointDensity").as<double>();
    }
    if (vm.count("wrongWayPenaltyCoeff")) {
        db::setting.wrongWayPenaltyCoeff = vm.at("wrongWayPenaltyCoeff").as<double>();
    }
    if (vm.count("fixOpenBySST")) {
        db::setting.fixOpenBySST = vm.at("fixOpenBySST").as<bool>();
    }
    // db
    if (vm.count("dbVerbose")) {
        db::setting.dbVerbose = db::VerboseLevelT::_from_string(vm.at("dbVerbose").as<std::string>().c_str());
    }
    if (vm.count("dbUsePoorViaMapThres")) {
        db::setting.dbUsePoorViaMapThres = vm.at("dbUsePoorViaMapThres").as<int>();
    }
    if (vm.count("dbPoorWirePenaltyCoeff")) {
        db::setting.dbPoorWirePenaltyCoeff = vm.at("dbPoorWirePenaltyCoeff").as<double>();
    }
    if (vm.count("dbPoorViaPenaltyCoeff")) {
        db::setting.dbPoorViaPenaltyCoeff = vm.at("dbPoorViaPenaltyCoeff").as<double>();
    }
    if (vm.count("dbNondefaultViaPenaltyCoeff")) {
        db::setting.dbNondefaultViaPenaltyCoeff = vm.at("dbNondefaultViaPenaltyCoeff").as<double>();
    }
    if (vm.count("dbInitHistUsageForPinAccess")) {
        db::setting.dbInitHistUsageForPinAccess = vm.at("dbInitHistUsageForPinAccess").as<double>();
    }

    // Read benchmarks
    Rsyn::ISPD2018Reader reader;
    const Rsyn::Json params = {
        {"lefFile", lefFile},
        {"defFile", defFile},
        {"guideFile", guideFile},
    };
    log() << std::endl;
    if (db::setting.dbVerbose >= +db::VerboseLevelT::HIGH) {
        log() << "################################################################" << std::endl;
        log() << "Start reading benchmarks" << std::endl;
    }
    reader.load(params);
    if (db::setting.dbVerbose >= +db::VerboseLevelT::HIGH) {
        log() << "Finish reading benchmarks" << std::endl;
        log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
              << std::endl;
        log() << std::endl;
    }

    // Route
    database.init();
    db::setting.adapt();
    Router router;
    router.run();
    database.writeNetTopo(db::setting.outputFile + ".topo");
    database.clear();
    database.writeDEF(db::setting.outputFile);
    log() << "Finish writing def" << std::endl;
    log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
          << std::endl;
    log() << std::endl;
}

// -----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    Rsyn::Session::init();

    printlog("------------------------------------------------------------------------------");
    printlog("                    ISPD 2019 - Detailed Routing Contest                      ");
    printlog("                             Team number : 15                                 ");
    printlog("                             Team name: Dr. CU                                ");
    printlog("        Members: Gengjie Chen, Haocheng Li, Bentian Jiang, Jingsong Chen,     ");
    printlog("                 Evangeline F.Y. Young                                        ");
    printlog("        Affiliation: The Chinese University of Hong Kong                      ");
    printlog("------------------------------------------------------------------------------");

    std::cout << std::boolalpha;  // set std::boolalpha to std::cout

    try {
        using namespace boost::program_options;
        options_description desc{"Options"};
        // clang-format off
        desc.add_options()
                ("help", "Help message.")
                ("lef", value<std::string>()->required(), "Input .lef file")
                ("def", value<std::string>()->required(), "Input .def file.")
                ("guide", value<std::string>()->required(), "Input .guide file")
                ("threads", value<int>()->required(), "# of threads")
                ("tat", value<int>()->required(), "Runtime limit (sec)")
                ("output", value<std::string>()->required(), "Output file name")
                // optional
                ("multiNetVerbose", value<std::string>())
                ("multiNetScheduleSortAll", value<bool>())
                ("multiNetScheduleReverse", value<bool>())
                ("multiNetScheduleSort", value<bool>())
                ("rrrIters", value<int>())
                ("rrrWriteEachIter", value<bool>())
                ("rrrInitVioCostDiscount", value<double>())
                ("rrrFadeCoeff", value<double>())
                ("defaultGuideExpand", value<int>())
                ("diffLayerGuideVioThres", value<int>())
                ("wrongWayPointDensity", value<double>())
                ("wrongWayPenaltyCoeff", value<double>())
                ("fixOpenBySST", value<bool>())
                ("dbVerbose", value<std::string>())
                ("dbUsePoorViaMapThres", value<int>())
                ("dbPoorWirePenaltyCoeff", value<double>())
                ("dbPoorViaPenaltyCoeff", value<double>())
                ("dbNondefaultViaPenaltyCoeff", value<double>())
                ("dbInitHistUsageForPinAccess", value<double>())
                ;
        // clang-format on
        variables_map vm;
        store(command_line_parser(argc, argv)
                  .options(desc)
                  .style(command_line_style::unix_style | command_line_style::allow_long_disguise)
                  .run(),
              vm);
        notify(vm);
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }
        for (const auto& option : desc.options()) {
            if (vm.count(option->long_name())) {
                std::string name = option->description().empty() ? option->long_name() : option->description();
                log() << std::left << std::setw(18) << name << ": ";
                const auto& value = vm.at(option->long_name()).value();
                if (auto v = boost::any_cast<double>(&value)) {
                    std::cout << *v;
                } else if (auto v = boost::any_cast<int>(&value)) {
                    std::cout << *v;
                } else if (auto v = boost::any_cast<std::string>(&value)) {
                    std::cout << *v;
                } else if (auto v = boost::any_cast<bool>(&value)) {
                    std::cout << *v;
                } else {
                    std::cout << "unresolved type";
                }
                std::cout << std::endl;
            }
        }
        runISPD18Flow(vm);
    } catch (const boost::program_options::error& e) {
        printlog(e.what());
    }

    printlog("---------------------------------------------------------------------------");
    printlog("                               Terminated...                               ");
    printlog("---------------------------------------------------------------------------");

    return 0;
}
