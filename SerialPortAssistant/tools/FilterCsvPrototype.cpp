#include "../SignalFilterPipeline.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct CsvSourceRow
{
    std::array<std::string, 4> columns;
    SignalFilterSample sample;
};

std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    std::istringstream stream(line);
    while (std::getline(stream, field, ',')) fields.push_back(field);
    if (!line.empty() && line.back() == ',') fields.emplace_back();
    return fields;
}

bool readInput(
    const std::filesystem::path& path,
    std::vector<CsvSourceRow>* rows,
    std::string* error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        *error = "cannot open input CSV";
        return false;
    }

    std::string line;
    if (!std::getline(input, line)) {
        *error = "input CSV is empty";
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() >= 3
        && static_cast<unsigned char>(line[0]) == 0xef
        && static_cast<unsigned char>(line[1]) == 0xbb
        && static_cast<unsigned char>(line[2]) == 0xbf) {
        line.erase(0, 3);
    }
    if (line != "Time(s),Voltage(V),Current(A),OpticalSignal") {
        *error = "unsupported CSV header";
        return false;
    }

    std::size_t lineNumber = 1;
    double previousTime = -1.0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const std::vector<std::string> fields = splitCsvLine(line);
        if (fields.size() < 4) {
            *error = "not enough columns at line " + std::to_string(lineNumber);
            return false;
        }

        try {
            CsvSourceRow row;
            for (std::size_t index = 0; index < row.columns.size(); ++index) {
                row.columns[index] = fields[index];
            }
            row.sample.timeSeconds = std::stod(fields[0]);
            row.sample.voltage = std::stod(fields[1]);
            row.sample.current = std::stod(fields[2]);
            row.sample.optical = std::stod(fields[3]);
            if (!std::isfinite(row.sample.timeSeconds)
                || !std::isfinite(row.sample.voltage)
                || !std::isfinite(row.sample.current)
                || !std::isfinite(row.sample.optical)
                || (!rows->empty() && row.sample.timeSeconds <= previousTime)) {
                throw std::runtime_error("invalid numeric value");
            }
            previousTime = row.sample.timeSeconds;
            rows->push_back(std::move(row));
        }
        catch (const std::exception&) {
            *error = "invalid numeric value at line " + std::to_string(lineNumber);
            return false;
        }
    }

    if (rows->empty()) {
        *error = "input CSV has no data rows";
        return false;
    }
    return true;
}

bool writeFiltered(
    const std::filesystem::path& path,
    const std::vector<CsvSourceRow>& rows,
    const SignalFilterBatchResult& result,
    std::string* error)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        *error = "cannot create filtered CSV";
        return false;
    }
    output << "Time(s),Voltage(V),Current(A),OpticalSignal,"
        "VoltageFiltered(V),CurrentFiltered(A),OpticalSignalFiltered,"
        "FilterValid,GPCIEvent\n";
    output << std::fixed << std::setprecision(9);
    for (std::size_t index = 0; index < rows.size(); ++index) {
        output << rows[index].columns[0] << ','
            << rows[index].columns[1] << ','
            << rows[index].columns[2] << ','
            << rows[index].columns[3] << ','
            << result.samples[index].voltage << ','
            << result.samples[index].current << ','
            << result.samples[index].optical << ','
            << (result.samples[index].filterValid ? 1 : 0) << ','
            << (result.samples[index].gpciEvent ? 1 : 0) << '\n';
    }
    if (!output) {
        *error = "failed while writing filtered CSV";
        return false;
    }
    return true;
}

bool writePulses(
    const std::filesystem::path& path,
    const std::vector<SignalPulseSummary>& pulses,
    std::string* error)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        *error = "cannot create pulse summary CSV";
        return false;
    }
    output << "PulseIndex,StartTime(s),EndTime(s),LocalBaseline,RawPeak,"
        "FilteredPeak,RawArea,FilteredArea,AreaDifference(%),Valid\n";
    output << std::fixed << std::setprecision(9);
    for (const SignalPulseSummary& pulse : pulses) {
        const double difference = std::fabs(pulse.rawArea) > 1e-18
            ? 100.0 * (pulse.filteredArea - pulse.rawArea) / pulse.rawArea
            : 0.0;
        output << pulse.index << ','
            << pulse.startTimeSeconds << ','
            << pulse.endTimeSeconds << ','
            << pulse.localBaseline << ','
            << pulse.rawPeak << ','
            << pulse.filteredPeak << ','
            << pulse.rawArea << ','
            << pulse.filteredArea << ','
            << difference << ','
            << (pulse.valid ? 1 : 0) << '\n';
    }
    if (!output) {
        *error = "failed while writing pulse summary CSV";
        return false;
    }
    return true;
}

} // namespace

int wmain(int argumentCount, wchar_t* arguments[])
{
    if (argumentCount != 4) {
        std::wcerr << L"Usage: FilterCsvPrototype input.csv filtered.csv pulses.csv\n";
        return 2;
    }

    std::vector<CsvSourceRow> sourceRows;
    std::string error;
    if (!readInput(arguments[1], &sourceRows, &error)) {
        std::cerr << "FAILED: " << error << '\n';
        return 1;
    }

    std::vector<SignalFilterSample> samples;
    samples.reserve(sourceRows.size());
    for (const CsvSourceRow& row : sourceRows) samples.push_back(row.sample);

    const SignalFilterPipeline pipeline;
    const SignalFilterBatchResult result = pipeline.process(samples);
    if (result.samples.size() != sourceRows.size()) {
        std::cerr << "FAILED: filtered row count mismatch\n";
        return 1;
    }
    if (!writeFiltered(arguments[2], sourceRows, result, &error)
        || !writePulses(arguments[3], result.pulses, &error)) {
        std::cerr << "FAILED: " << error << '\n';
        return 1;
    }

    std::cout << "Filtered " << result.samples.size()
        << " rows and detected " << result.pulses.size() << " GPCI pulses.\n";
    return 0;
}
