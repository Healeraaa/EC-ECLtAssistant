#ifndef REALTIMESIGNALFILTER_H
#define REALTIMESIGNALFILTER_H

#include "SignalFilterPipeline.h"

#include <cstdint>
#include <map>
#include <vector>

struct RealtimeFilteredSample
{
    SignalFilterSample raw;
    SignalFilterResult filtered;
};

class RealtimeSignalFilter
{
public:
    explicit RealtimeSignalFilter(const SignalFilterSettings& settings = {});

    void reset();
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    std::vector<RealtimeFilteredSample> appendIv(
        double timeSeconds,
        double voltage,
        double current);
    std::vector<RealtimeFilteredSample> appendLight(
        double timeSeconds,
        double optical);
    std::vector<RealtimeFilteredSample> flush();

    std::size_t pendingRows() const { return m_partialRows.size(); }
    std::size_t bufferedSamples() const { return m_window.size(); }
    std::uint64_t droppedPartialRows() const { return m_droppedPartialRows; }

private:
    struct PartialRow
    {
        SignalFilterSample sample;
        bool hasIv = false;
        bool hasLight = false;
    };

    static std::int64_t timeKey(double timeSeconds);
    std::vector<RealtimeFilteredSample> collectReadyRows();
    std::vector<RealtimeFilteredSample> produceFiltered(bool flushTail);
    std::vector<RealtimeFilteredSample> bypassSamples(
        const std::vector<SignalFilterSample>& samples) const;

    SignalFilterSettings m_settings;
    SignalFilterPipeline m_pipeline;
    std::map<std::int64_t, PartialRow> m_partialRows;
    std::vector<SignalFilterSample> m_window;
    std::int64_t m_latestIvKey = 0;
    std::int64_t m_latestLightKey = 0;
    bool m_hasIv = false;
    bool m_hasLight = false;
    bool m_enabled = true;
    bool m_streamStarted = false;
    std::size_t m_emittedPrefix = 0;
    std::uint64_t m_droppedPartialRows = 0;
};

#endif // REALTIMESIGNALFILTER_H
