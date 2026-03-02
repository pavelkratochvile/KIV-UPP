#include"../hpp/serial.hpp"
#include<iostream>
#include<sstream>
#include<fstream>
#include<string>
#include<vector>
#include <climits>
#include<mutex>

namespace {
    constexpr float kMinSentinel = std::numeric_limits<float>::max();
    constexpr float kMaxSentinel = std::numeric_limits<float>::lowest();
}

int Serial::Preprocess() {  
    if (!std::filesystem::exists(filePath)) {
        return 1;
    }
    // Jen zavolá utilitu pro předzpracování s celým souborem jako intervalem, protože v sériové verzi není potřeba dělení do více vláken
    PreprocessByInterval(0, INT_MAX, filePath, data, dataMutex, 0);
    if (data.localMins[0] == kMinSentinel || data.localMaxs[0] == kMaxSentinel) {
        return 2;
    }
    return 0;
}

int Serial::deflectionIdentification(std::filesystem::path outputDir) {
    if (outputDir.empty() || data.stations.empty()) {
        return 1;
    }
    std::filesystem::path deflectionFile = outputDir / "vykyvy.csv";
    std::ofstream deflectionFileStream(deflectionFile);
    if (!deflectionFileStream.is_open()) {
        std::cerr << "Failed to create deflection file: " << deflectionFile << std::endl;
        return 2;
    }

    std::mutex deflectionFileMutexFake;

    // Seriove volani utility pro identifikaci odchylek pro každou stanici, protože v sériové verzi není potřeba synchronizace přístupu k souboru, ale pro konzistenci s paralelní verzí je zde použit mutex, i když není skutečně potřeba
    for(Station& station : data.stations) {
        deflectionIdentificationUtil(station, deflectionFileStream, deflectionFileMutexFake);
    }

    return std::filesystem::exists(deflectionFile) ? 0 : 3;
}

int Serial::addStationsInfoFromFile(std::filesystem::path filePath){
    if (!std::filesystem::exists(filePath)) {
        return 1;
    }
    addStationsInfo(filePath, data);
    return data.stationsInfo.empty() ? 2 : 0;
}

int Serial::findGlobalExtremes(){
    if (data.localMins[0] == kMinSentinel || data.localMaxs[0] == kMaxSentinel) {
        return 1;
    }
    // Nastavi globalni extremy jako prvni lokalni extrem pro kazdy mesic, protoze je tam v seriove verzi ukladame
    data.globalMax = data.localMaxs[0];
    data.globalMin = data.localMins[0];
    return data.globalMax < data.globalMin ? 2 : 0;
}

int Serial::drawSVGs(std::filesystem::path outDir, std::filesystem::path svgTemplate, int year){
    if (outDir.empty() || !std::filesystem::exists(svgTemplate) || year < 0) {
        return 1;
    }
    // Seriove vykresli SVG pro každý měsíc, protože v sériové verzi není potřeba dělení do více vláken
    for(int month = 1; month <= 12; month++){
        drawMonthSVG(year, month, data, outDir, svgTemplate);
    }

    return std::filesystem::exists(outDir / "leden.svg") ? 0 : 2;
}