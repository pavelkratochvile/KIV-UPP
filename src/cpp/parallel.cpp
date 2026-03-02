#include"../hpp/parallel.hpp"
#include"../hpp/utils.hpp"
#include<iostream>
#include<thread>
#include<fstream>
#include<sstream>

namespace {
    constexpr float kMinSentinel = std::numeric_limits<float>::max();
    constexpr float kMaxSentinel = std::numeric_limits<float>::lowest();
}

/**
 * Metoda posouva ukazatel v souboru na začátek řádku, aby se zajistilo, že zpracování dat začíná vždy od začátku řádku. Tato metoda je použita při dělení souboru do intervalů pro paralelní zpracování, aby se zabránilo rozdělení řádků mezi vlákna.
 * @param file Ukazatel na otevřený soubor
 * @param rawOffset Počáteční pozice v souboru, kterou je třeba zarovnat na začátek řádku
 * @param fileSize Velikost souboru, aby se zabránilo posouvání za konec souboru
 * @return Zarovnaná pozice v souboru, která je na začátku řádku
 */
static long alignToLineStart(FILE* file, long rawOffset, long fileSize) {
    // Pokud je rawOffset na začátku souboru nebo mimo rozsah, vrátíme ho bez změny
    if (rawOffset <= 0) {
        return 0;
    }
    // Pokud je rawOffset za koncem souboru, vrátíme velikost souboru
    if (rawOffset >= fileSize) {
        return fileSize;
    }
    // Posuneme se na rawOffset a zkontrolujeme, zda jsme na začátku řádku
    if (fseek(file, rawOffset - 1, SEEK_SET) != 0) {
        return rawOffset;
    }

    // Pokud je předchozí znak nový řádek, jsme již na začátku řádku, takže vrátíme rawOffset
    int prevChar = fgetc(file);
    if (prevChar == '\n') {
        return rawOffset;
    }

    // Jinak posouváme ukazatel dopředu, dokud nenarazíme na nový řádek nebo konec souboru
    int currentChar;
    while ((currentChar = fgetc(file)) != EOF) {
        if (currentChar == '\n') {
            return ftell(file);
        }
    }

    return fileSize;
}

// Pomocná funkce pro identifikaci výkyvů, která bude spuštěna v každém vlákně pro zpracování stanic. Tato funkce načítá stanice z objektu Data a identifikuje výkyvy pro každou stanici, přičemž synchronizuje přístup k souboru s výsledky pomocí mutexu deflectionFileMutex.
static void deflectionWorker(Parallel& parallel, std::ofstream& deflectionFileStream) {
    while (true) {
        parallel.stationIndexMutex.lock();
        // Pokud jsme zpracovali všechny stanice, uvolníme mutex a ukončíme vlákno
        if (parallel.stationTakingIndex >= static_cast<int>(parallel.data.stations.size())) {
            parallel.stationIndexMutex.unlock();
            return;
        }
        int stationIndex = parallel.stationTakingIndex++;
        parallel.stationIndexMutex.unlock();

        deflectionIdentificationUtil(parallel.data.stations[stationIndex], deflectionFileStream, parallel.deflectionFileMutex);
    }
}

int Parallel::Preprocess(){
    if (!std::filesystem::exists(filePath) || numThreads <= 0) {
        return 1;
    }
    FILE* file = fopen(filePath.c_str(), "r");
    if (file == nullptr) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return 2;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fclose(file);

    file = fopen(filePath.c_str(), "r");
    if (file == nullptr) {
        std::cerr << "Failed to reopen file: " << filePath << std::endl;
        return 3;
    }

    long intervalSize = fileSize / numThreads;
    std::vector<long> boundaries(numThreads + 1);
    boundaries[0] = 0;
    boundaries[numThreads] = fileSize;

    std::vector<std::thread> threads;

    // Pro každý interval vypočítáme zarovnanou pozici na začátku řádku a uložíme ji do vektoru boundaries
    for(int i = 1; i < numThreads; ++i) {
        long rawBoundary = i * intervalSize;
        boundaries[i] = alignToLineStart(file, rawBoundary, fileSize);
    }

    // Spustíme vlákna pro předzpracování dat, přičemž každé vlákno zpracovává svůj vlastní interval souboru, který je zarovnán na začátek řádku, aby se zabránilo rozdělení řádků mezi vlákna
    for(int i = 0; i < numThreads; ++i) {
        threads.emplace_back(PreprocessByInterval, boundaries[i], boundaries[i + 1], filePath, std::ref(data), std::ref(dataMutex), i);
    }

    // Po spuštění všech vláken počkáme na jejich dokončení pomocí join, aby se zajistilo, že všechny části souboru jsou zpracovány, než pokračujeme s dalšími kroky zpracování dat
    for (auto& thread : threads) {
        if(thread.joinable()) {
            thread.join();
        }
    }

    fclose(file);
    for (int i = 0; i < numThreads; ++i) {
        if (data.localMins[i] != kMinSentinel || data.localMaxs[i] != kMaxSentinel) {
            return 0;
        }
    }
    return 4;
}

int Parallel::deflectionIdentification(std::filesystem::path outputDir){
    if (outputDir.empty() || data.stations.empty()) {
        return 1;
    }
    std::filesystem::path deflectionFile = outputDir / "vykyvy.csv";
    std::ofstream deflectionFileStream(deflectionFile);
    if (!deflectionFileStream.is_open()) {
        std::cerr << "Failed to create deflection file: " << deflectionFile << std::endl;
        return 2;
    }
    
    this->stationTakingIndex = 0;
    std::vector<std::thread> threads;

    // Spustíme vlákna pro identifikaci výkyvů, přičemž každé vlákno zpracovává stanice z objektu Data, přičemž synchronizuje přístup k souboru s výsledky pomocí mutexu deflectionFileMutex, aby se zabránilo konfliktům při zápisu výsledků
    for(int i = 0; i < numThreads; ++i) {
        threads.emplace_back(deflectionWorker, std::ref(*this), std::ref(deflectionFileStream));
    }

    // Po spuštění všech vláken počkáme na jejich dokončení pomocí join, aby se zajistilo, že všechny stanice jsou zpracovány a všechny výsledky jsou zapsány do souboru s výsledky identifikace výkyvů, než pokračujeme s dalšími kroky zpracování dat
    for (auto& thread : threads){
        thread.join();
    }

    return std::filesystem::exists(deflectionFile) ? 0 : 3;
}

int Parallel::drawSVGs(std::filesystem::path outDir, std::filesystem::path svgTemplate, int year){
    if (outDir.empty() || !std::filesystem::exists(svgTemplate) || year < 0) {
        return 1;
    }
    std::vector<std::thread> threads;
    // Spustíme vlákna pro vykreslení SVG pro každý měsíc, přičemž každé vlákno zpracovává jeden měsíc, přičemž synchronizuje přístup k objektu Data pomocí mutexu dataMutex, aby se zabránilo konfliktům při čtení informací o stanicích
    for(int month = 1; month <= 12; month++){
        threads.emplace_back(drawMonthSVG, year, month, std::ref(data), outDir, svgTemplate);
    }
    for(auto& thread : threads) {
        thread.join();
    }

    return std::filesystem::exists(outDir / "leden.svg") ? 0 : 2;
}

int Parallel::addStationsInfoFromFile(std::filesystem::path filePath){
    if (!std::filesystem::exists(filePath)) {
        return 1;
    }
    // zavolá utilitu pro přidání informací o stanicích z daného souboru do objektu Data, který je sdílen mezi vlákny, aby obsahoval všechny potřebné informace o stanicích pro další zpracování
    addStationsInfo(filePath, data);
    return data.stationsInfo.empty() ? 2 : 0;
}

int Parallel::findGlobalExtremes(){
    // Nastavi globalni extremy jako prvni lokalni extrem pro kazdy mesic, protoze je tam v paralelni verzi ukladame
    float gMax = std::numeric_limits<float>::lowest();
    float gMin = std::numeric_limits<float>::max();

    for(int i = 0; i < 12; ++i) {
        if(data.localMins[i] < gMin) {
            gMin = data.localMins[i];
        }
        if(data.localMaxs[i] > gMax) {
            gMax = data.localMaxs[i];
        }
    }
    data.globalMin = gMin;
    data.globalMax = gMax;
    return data.globalMax < data.globalMin ? 1 : 0;
}