#include "Router.h"
#include "Scheduler.h"

const MTStat& MTStat::operator+=(const MTStat& rhs) {
    auto dur = rhs.durations;
    std::sort(dur.begin(), dur.end());
    if (durations.size() < dur.size()) {
        durations.resize(dur.size(), 0.0);
    }
    for (int i = 0; i < dur.size(); ++i) {
        durations[i] += dur[i];
    }
    return *this;
}

ostream& operator<<(ostream& os, const MTStat mtStat) {
    double minDur = std::numeric_limits<double>::max(), maxDur = 0.0, avgDur = 0.0;
    for (double dur : mtStat.durations) {
        minDur = min(minDur, dur);
        maxDur = max(maxDur, dur);
        avgDur += dur;
    }
    avgDur /= mtStat.durations.size();
    os << "#threads=" << mtStat.durations.size() << " (dur: min=" << minDur << ", max=" << maxDur << ", avg=" << avgDur
       << ")";
    return os;
}

void Router::run() {
    allNetStatus.resize(database.nets.size(), db::RouteStatus::FAIL_UNPROCESSED);
    for (iter = 0; iter < db::setting.rrrIterLimit; iter++) {
        db::rrrIter = iter;
        log() << std::endl;
        log() << "################################################################" << std::endl;
        log() << "Start RRR iteration " << iter << std::endl;
        log() << std::endl;
        db::routeStat.clear();
        vector<int> netsToRoute = getNetsToRoute();
        if (netsToRoute.empty()) {
            if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
                log() << "No net is identified for this iteration of RRR." << std::endl;
                log() << std::endl;
            }
            break;
        }
        if (iter > 0) {
            // updateCost should before ripup, otherwise, violated nets have gone
            updateCost();
            ripup(netsToRoute);
        }
        if (db::setting.rrrIterLimit > 1) {
            double step = (1.0 - db::setting.rrrInitVioCostDiscount) / (db::setting.rrrIterLimit - 1);
            database.setUnitVioCost(db::setting.rrrInitVioCostDiscount + step * iter);
        }
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            log() << "defaultGuideExpand = " << db::setting.defaultGuideExpand << std::endl;
        }
        route(netsToRoute);
        log() << std::endl;
        log() << "Finish RRR iteration " << iter << std::endl;
        log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
              << std::endl;
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            printStat(db::setting.rrrWriteEachIter);
        }
        if (db::setting.rrrWriteEachIter) {
            std::string fn = "iter" + std::to_string(iter) + "_" + db::setting.outputFile;
            printlog("Write result of RRR iter", iter, "to", fn, "...");
            finish();
            database.writeDEF(fn);
            unfinish();
        }
    }
    finish();
    log() << std::endl;
    log() << "################################################################" << std::endl;
    log() << "Finish all RRR iterations and PostRoute" << std::endl;
    log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
          << std::endl;
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printStat(true);
    }
}

vector<int> Router::getNetsToRoute() {
    vector<int> netsToRoute;
    if (iter == 0) {
        for (int i = 0; i < database.nets.size(); i++) {
            // if (database.nets[i].getName() == "net8984") netsToRoute.push_back(i);
            netsToRoute.push_back(i);
        }
    } else {
        for (const auto& net : database.nets) {
            if (database.getNetVioCost(net) > 0.0) {
                netsToRoute.push_back(net.idx);
            }
        }
    }

    return netsToRoute;
}

void Router::ripup(const vector<int>& netsToRoute) {
    for (auto netIdx : netsToRoute) {
        UpdateDB::clearRouteResult(database.nets[netIdx]);
        allNetStatus[netIdx] = db::RouteStatus::FAIL_UNPROCESSED;
    }
}

void Router::updateCost() {
    database.addHistCost();
    database.fadeHistCost();
    db::setting.defaultGuideExpand += iter * 2;
}

void Router::route(const vector<int>& netsToRoute) {
    // init SingleNetRouters
    vector<SingleNetRouter> routers;
    routers.reserve(netsToRoute.size());
    for (int netIdx : netsToRoute) {
        routers.emplace_back(database.nets[netIdx]);
    }

    // pre route
    auto preMT = runJobsMT(netsToRoute.size(), [&](int netIdx) { routers[netIdx].preRoute(); });
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("preMT", preMT);
        printStat();
    }

    // schedule
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Start multi-thread scheduling. There are " << netsToRoute.size() << " nets to route." << std::endl;
    }
    Scheduler scheduler(routers);
    const vector<vector<int>>& batches = scheduler.schedule();
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Finish multi-thread scheduling" << ((db::setting.numThreads == 0) ? " using simple mode" : "")
              << ". There will be " << batches.size() << " batches." << std::endl;
        log() << std::endl;
    }

    // maze route and commit DB by batch
    int iBatch = 0;
    MTStat allMazeMT, allCommitMT;
    // ofstream timeOfs(db::setting.outputFile + ".time");
    // timeOfs << "batch\tnet_idx\tnet_name\tvertex\tpin\ttime" << std::endl;
    // mutex timeMutex;
    for (const vector<int>& batch : batches) {
        // maze route
        auto mazeMT = runJobsMT(batch.size(), [&](int jobIdx) {
            int routerIdx = batch[jobIdx];
            auto& router = routers[routerIdx];
            // utils::timer mzTimer;
            router.mazeRoute();
            /*
            timeMutex.lock();
            timeOfs << iBatch << '\t' << router.localNet.idx << '\t' << router.localNet.getName() << '\t'
                    << router.localNet.estimatedNumOfVertices << '\t' << router.localNet.rsynPins.size() << '\t'
                    << mzTimer.elapsed() << std::endl;
            timeMutex.unlock();
            */
            allNetStatus[router.dbNet.idx] = router.status;
        });
        allMazeMT += mazeMT;
        // commit nets to DB
        auto commitMT = runJobsMT(batch.size(), [&](int jobIdx) {
            int routerIdx = batch[jobIdx];
            if (!isSucc(routers[routerIdx].status)) return;
            routers[routerIdx].commitNetToDB();
        });
        allCommitMT += commitMT;
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::HIGH && db::setting.numThreads != 0) {
            log() << "Batch " << iBatch << " done: size=" << batch.size() << ", mazeMT " << mazeMT << ", commitMT "
                  << commitMT << std::endl;
        }
        iBatch++;
    }
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("allMazeMT", allMazeMT);
        printlog("allCommitMT", allCommitMT);
    }
}

void Router::finish() {
    // post route
    auto postMT = runJobsMT(database.nets.size(), [&](int netIdx) {
        if (!isSucc(allNetStatus[netIdx])) return;
        PostRoute postRoute(database.nets[netIdx]);
        postRoute.run();
    });
    printlog("postMT", postMT);
    // final open fix
    if (db::setting.fixOpenBySST) {
        int count = 0;
        for (auto& net : database.nets) {
            if (net.topo.empty() && net.numOfPins() > 1) {
                connectBySTT(net);
                count++;
            }
        }
        if (count > 0) log() << "#nets connected by STT: " << count << std::endl;
    }
}

void Router::unfinish() {
    runJobsMT(database.nets.size(), [&](int netIdx) { database.nets[netIdx].clearPostRouteResult(); });
}

void Router::printStat(bool major) {
    log() << std::endl;
    log() << "----------------------------------------------------------------" << std::endl;
    db::routeStat.print();
    if (major) {
        database.printAllUsageAndVio();
    }
    log() << "----------------------------------------------------------------" << std::endl;
    log() << std::endl;
}

MTStat Router::runJobsMT(int numJobs, const std::function<void(int)>& handle) {
    int numThreads = min(numJobs, db::setting.numThreads);
    MTStat mtStat(max(1, db::setting.numThreads));
    if (numThreads <= 1) {
        utils::timer threadTimer;
        for (int i = 0; i < numJobs; ++i) {
            handle(i);
        }
        mtStat.durations[0] = threadTimer.elapsed();
    } else {
        int globalJobIdx = 0;
        std::mutex mtx;
        utils::timer threadTimer;
        auto thread_func = [&](int threadIdx) {
            int jobIdx;
            while (true) {
                mtx.lock();
                jobIdx = globalJobIdx++;
                mtx.unlock();
                if (jobIdx >= numJobs) {
                    mtStat.durations[threadIdx] = threadTimer.elapsed();
                    break;
                }
                handle(jobIdx);
            }
        };

        std::thread threads[numThreads];
        for (int i = 0; i < numThreads; i++) {
            threads[i] = std::thread(thread_func, i);
        }
        for (int i = 0; i < numThreads; i++) {
            threads[i].join();
        }
    }
    return mtStat;
}
