#pragma once

#include <map>

#include <boost/thread/mutex.hpp>


class Repeater {
public:
    Repeater();

    //! Run indefinitely or until we quit
    int run(int argc, char *argv[]);

    //! Signal that it's time to shutdown, from another thread
    void shutdown();

    //! Gain mode
    enum Mode {
        M_GAIN, //!< Simple gain
        M_TARGET, //!< Target power level
        M_FEEDBACK, //!< Dynamic feedback level
    };

    //! Get the gain mode
    Mode getMode() const;

    //! Set the gain mode
    void setMode(Mode);

    //! Get the gain level for the specified mode
    float getModeLevel(Mode) const;

    //! Set the gain level for the specified mode
    void setModeLevel(Mode, float);

    //! Get the gain dampening factor
    float getDampenFactor() const;

    //! Set the gain dampening factor
    void setDampenFactor(float);

    //! Loop time, in seconds
    float getLoopTime() const;

    //! Limiter power level
    float getLimiterLevel() const;

    //! Set limiter power level
    void setLimiterLevel(float);

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
    };

    //! Get a copy of the current history snapshot
    void getHistory(History&) const;

private:
    mutable boost::mutex mConfigMutex;
    Mode mMode;
    float mDampenFactor;
    unsigned int mSampleRate;
    float mLoopTime;
    float mLimiter;
    std::map<Mode, float> mLevels;
    bool mShutdown;

    mutable boost::mutex mHistoryMutex;
    History mHistory;

    // Current write position in the history
    History::DataPoints::size_type mHistPos;
    //! Total data for the current history datapoint
    History::DataPoint mCurData;
    //! Number of data points
    size_t mCurDataSamples;
};

    
