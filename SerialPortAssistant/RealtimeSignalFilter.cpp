#include "RealtimeSignalFilter.h"

#include <algorithm>
#include <cmath>
#include <iterator>

RealtimeSignalFilter::RealtimeSignalFilter(const SignalFilterSettings& settings)
    : m_settings(settings),
      m_pipeline(settings)
{
    if (m_settings.blockSize < 16) m_settings.blockSize = 128;
    if (m_settings.hopSize == 0 || m_settings.hopSize > m_settings.blockSize) {
        m_settings.hopSize = m_settings.blockSize / 2;
    }
}

void RealtimeSignalFilter::reset()
{
    m_partialRows.clear();
    m_window.clear();
    m_latestIvKey = 0;
    m_latestLightKey = 0;
    m_hasIv = false;
    m_hasLight = false;
    m_streamStarted = false;
    m_emittedPrefix = 0;
    m_droppedPartialRows = 0;
}

std::vector<RealtimeFilteredSample> RealtimeSignalFilter::appendIv(
    double timeSeconds,
    double voltage,
    double current)
{
    if (!std::isfinite(timeSeconds)
        || !std::isfinite(voltage)
        || !std::isfinite(current)) {
        return {};
    }
    const std::int64_t key = timeKey(timeSeconds);
    PartialRow& row = m_partialRows[key];
    row.sample.timeSeconds = timeSeconds;
    row.sample.voltage = voltage;
    row.sample.current = current;
    row.hasIv = true;
    m_latestIvKey = m_hasIv ? std::max(m_latestIvKey, key) : key;
    m_hasIv = true;
    return collectReadyRows();
}

std::vector<RealtimeFilteredSample> RealtimeSignalFilter::appendLight(
    double timeSeconds,
    double optical)
{
    if (!std::isfinite(timeSeconds) || !std::isfinite(optical)) return {};
    const std::int64_t key = timeKey(timeSeconds);
    PartialRow& row = m_partialRows[key];
    row.sample.timeSeconds = timeSeconds;
    row.sample.optical = optical;
    row.hasLight = true;
    m_latestLightKey = m_hasLight ? std::max(m_latestLightKey, key) : key;
    m_hasLight = true;
    return collectReadyRows();
}

std::vector<RealtimeFilteredSample> RealtimeSignalFilter::flush()
{
    for (auto iterator = m_partialRows.begin(); iterator != m_partialRows.end();) {
        if (iterator->second.hasIv && iterator->second.hasLight) {
            m_window.push_back(iterator->second.sample);
        }
        else {
            ++m_droppedPartialRows;
        }
        iterator = m_partialRows.erase(iterator);
    }
    return produceFiltered(true);
}

std::int64_t RealtimeSignalFilter::timeKey(double timeSeconds)
{
    return static_cast<std::int64_t>(std::llround(timeSeconds * 1000000.0));
}

std::vector<RealtimeFilteredSample> RealtimeSignalFilter::collectReadyRows()
{
    if (!m_hasIv || !m_hasLight) return {};
    const std::int64_t completeThrough = std::min(m_latestIvKey, m_latestLightKey);
    auto iterator = m_partialRows.begin();
    while (iterator != m_partialRows.end() && iterator->first <= completeThrough) {
        if (iterator->second.hasIv && iterator->second.hasLight) {
            m_window.push_back(iterator->second.sample);
        }
        else {
            ++m_droppedPartialRows;
        }
        iterator = m_partialRows.erase(iterator);
    }
    return produceFiltered(false);
}

std::vector<RealtimeFilteredSample> RealtimeSignalFilter::produceFiltered(bool flushTail)
{
    std::vector<RealtimeFilteredSample> output;
    if (!m_enabled) {
        output = bypassSamples(m_window);
        m_window.clear();
        m_streamStarted = false;
        m_emittedPrefix = 0;
        return output;
    }

    if (!m_streamStarted && m_window.size() >= m_settings.hopSize) {
        std::vector<SignalFilterSample> initialBlock(
            m_window.begin(),
            m_window.begin() + m_settings.hopSize);
        const SignalFilterBatchResult filtered = m_pipeline.process(initialBlock);
        for (std::size_t index = 0; index < filtered.samples.size(); ++index) {
            output.push_back({ initialBlock[index], filtered.samples[index] });
        }
        m_streamStarted = true;
        m_emittedPrefix = m_settings.hopSize;
    }

    while (m_streamStarted && m_window.size() >= m_settings.blockSize) {
        std::vector<SignalFilterSample> block(
            m_window.begin(),
            m_window.begin() + m_settings.blockSize);
        const SignalFilterBatchResult filtered = m_pipeline.process(block);
        for (std::size_t index = m_settings.hopSize; index < filtered.samples.size(); ++index) {
            output.push_back({ block[index], filtered.samples[index] });
        }
        m_window.erase(m_window.begin(), m_window.begin() + m_settings.hopSize);
        m_emittedPrefix = m_settings.hopSize;
    }

    if (flushTail && m_window.size() > m_emittedPrefix) {
        const SignalFilterBatchResult filtered = m_pipeline.process(m_window);
        for (std::size_t index = m_emittedPrefix; index < filtered.samples.size(); ++index) {
            SignalFilterResult sampleResult = filtered.samples[index];
            if (m_streamStarted) sampleResult.filterValid = true;
            output.push_back({ m_window[index], sampleResult });
        }
    }

    if (flushTail) {
        m_window.clear();
        m_streamStarted = false;
        m_emittedPrefix = 0;
    }
    return output;
}

std::vector<RealtimeFilteredSample> RealtimeSignalFilter::bypassSamples(
    const std::vector<SignalFilterSample>& samples) const
{
    std::vector<RealtimeFilteredSample> output;
    output.reserve(samples.size());
    for (const SignalFilterSample& sample : samples) {
        SignalFilterResult result;
        result.voltage = sample.voltage;
        result.current = sample.current;
        result.optical = sample.optical;
        result.filterValid = true;
        result.gpciEvent = std::fabs(sample.voltage) > m_settings.gpciVoltageThreshold;
        output.push_back({ sample, result });
    }
    return output;
}
