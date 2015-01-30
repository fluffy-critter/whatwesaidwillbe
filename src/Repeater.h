#pragma once

#include <boost/thread/mutex.hpp>
#include <boost/atomic.hpp>

#include <map>
#include <memory>

class Repeater {
public:
    //! Gain mode
    enum Mode {
        M_GAIN, //!< Simple gain
        M_TARGET, //!< Target power level
        M_FEEDBACK, //!< Dynamic feedback level
    };
    
    //! startup options
    struct Options {
        unsigned int sampleRate;
        size_t bufSize;
        size_t historySize;
        float loopDelay;
        int latencyALSA;
        std::string captureDevice, playbackDevice;
        std::string recDumpFile, listenDumpFile;
        Options():
            sampleRate(44100),
            bufSize(1024),
            historySize(2048),
            loopDelay(10.0),
            latencyALSA(120000),
            captureDevice("default"),
            playbackDevice("default")
        {}
    };

    //! runtime options
    struct Knobs {
        float dampen;
        float feedbackThreshold;
        float limitPower;
        Mode mode;
        std::map<Mode, float> levels;

        Knobs():
            dampen(0.99),
            feedbackThreshold(-1),
            limitPower(0.2),
            mode(M_GAIN)
        {
            levels[M_GAIN] = 1;
            levels[M_TARGET] = 0.1;
            levels[M_FEEDBACK] = 0.5;
        }
    };

    Repeater(const Options&, const Knobs&);

    typedef std::shared_ptr<Repeater> Ptr;

    //! Run indefinitely or until we quit
    int run();

    enum State {
        S_STARTUP,
        S_RUNNING,
        S_SHUTDOWN_REQUESTED,
        S_SHUTTING_DOWN,
        S_GONE
    };
    State getState() const;

    //! Signal that it's time to shutdown, from another thread
    void shutdown();

    const Options& getOptions() const { return mOptions; }

    const Knobs& getKnobs() const { return mKnobs; }
    void setKnobs(const Knobs&);

    struct History {
        //! Information about a single point in time
        struct DataPoint {
            //! The specified mode at the time
            Mode mode;

            //! Recorded power level
            float recordedPower;

            //! Expected power level
            float expectedPower;

            //! Limiter level
            float limitPower;

            //! Target gain (per model)
            float targetGain;

            //! Actual gain (per limiter and damping)
            float actualGain;

            DataPoint();
        };

        typedef std::vector<DataPoint> DataPoints;

        //! All the stored historical data
        DataPoints history;

        //! Play head index (latency-corrected)
        DataPoints::size_type playPos;

        //! Record head index
        DataPoints::size_type recordPos;

	History();
    };

    //! Get a copy of the current history snapshot
    void getHistory(History&) const;

private:
    Options mOptions;

    Knobs mKnobs, mKnobsNext;
    boost::atomic<bool> mKnobsUpdated;

    boost::atomic<State> mState;

    mutable boost::mutex mHistoryMutex;
    History mHistory;

    // Current write position in the history
    History::DataPoints::size_type mHistPos;
    //! Total data for the current history datapoint
    History::DataPoint mCurData;
    //! Number of data points
    size_t mCurDataSamples;
};

    
