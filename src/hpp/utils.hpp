#pragma once

#include<iostream>
#include<string>
#include<vector>
#include<array>
#include<sstream>
#include<cstdio>
#include<chrono>
#include<map>
#include<mutex>
#include<filesystem>
#include<limits>

/**
 * Třída Station reprezentuje jednotlivou meteorologickou stanici, která obsahuje ID stanice a mapu pro ukládání měsíčních dat.
 * Mapa monthlyData je strukturována jako map<int, map<int, pair<double, int>>>, kde klíč prvního mapování je rok, klíč druhého mapování je měsíc a hodnota je pár obsahující součet teplot a počet měření pro daný měsíc.
 * Třída obsahuje metodu addDailyMeasurement pro přidání denního měření a metodu calculateMonthlyAverages pro výpočet průměrné měsíční teploty.
 */
class Station{
    public:
        Station() {}
        int id;
        std::map<int, std::map<int, std::pair<double, int>>> monthlyData;
        
        void addDailyMeasurement(int year, int month, double temperature) {
            auto& stats = monthlyData[year][month];
            stats.first += temperature;
            stats.second += 1;
        }
        void calculateMonthlyAverages() {
            for (auto& yearEntry : monthlyData) {
                for (auto& monthEntry : yearEntry.second) {
                    auto& stats = monthEntry.second;
                    if (stats.second > 0) {
                        stats.first /= stats.second;
                    }
                }
            }
        }
};

/**
 * Třída StationInfo reprezentuje informace o meteorologické stanici, včetně názvu, zeměpisné šířky a délky.
 * Třída Data slouží jako kontejner pro globální a lokální extrémy, seznam stanic a mapu informací o stanicích. Obsahuje metodu addStationInfo pro přidání informací o stanici do mapy stationsInfo.
 */
class StationInfo{
    public:
        StationInfo(std::string name, float latitude, float longitude);
        std::string name;
        float latitude;
        float longitude;
};

/**
 * Třída Data slouží jako kontejner pro globální a lokální extrémy, seznam stanic a mapu informací o stanicích. Obsahuje metodu addStationInfo pro přidání informací o stanici do mapy stationsInfo.
 */
class Data{
    public:
        float globalMax;
        float globalMin;
        std::vector<float> localMins = std::vector<float>(12, std::numeric_limits<float>::max());
        std::vector<float> localMaxs = std::vector<float>(12, std::numeric_limits<float>::lowest());
        std::vector<Station> stations;
        std::map<int, StationInfo> stationsInfo;
        void addStationInfo(int id, std::string name, float latitude, float longitude);
};

/**
 * Třída RGB reprezentuje barvu v RGB formátu s třemi složkami: r (červená), g (zelená) a b (modrá). Konstruktor umožňuje inicializaci těchto složek.
 */
class RGB{
    public:
        RGB(int r, int g, int b) : r(r), g(g), b(b) {}
        int r;
        int g;
        int b;
};

/**
 * Třída MonthNameLookup poskytuje statickou metodu getMonthName, která vrací název měsíce v češtině na základě zadaného čísla měsíce (1-12). Pokud je zadané číslo mimo tento rozsah, metoda vrací "Unknown".
 */
class MonthNameLookup {
    public:
        static std::string getMonthName(int month) {
            static const std::array<std::string, 12> monthNames = {
                "leden", "únor", "březen", "duben", "květen", "červen",
                "červenec", "srpen", "září", "říjen", "listopad", "prosinec"
            };
            if (month >= 1 && month <= 12) {
                return monthNames[month - 1];
            }
            return "Unknown";
        }
};

/**
 * Přidá informace o meteorologických stanicích ze souboru do objektu Data.
 * @param filePath Cesta k souboru s informacemi o stanicích.
 * @param data Reference na objekt Data, do kterého budou informace přidány.
 */
void addStationsInfo(std::filesystem::path filePath, Data& data);

/**
 * Převede datum na počet dní od počátku roku 1.
 * @param y Rok
 * @param m Měsíc
 * @param d Den
 * @return Počet dní od počátku roku 1
 */
long dateToDays(int y, int m, int d);

/**
 * Najde lokální extrémy pro danou řádku dat a aktualizuje je v objektu Data.
 * @param line Řádka dat obsahující měření
 */
void findExtremes(std::string line, Data& data, int threadIndex);

/**
 * Přečte data z daného souboru v zadaném rozsahu řádků, najde lokální extrémy a přidá stanice s pěti lety kontinuálních dat a průměrem alespoň 100 dní měření za rok do objektu Data.
 */
void addIfHasFiveContinuousYears(std::vector<std::string> lines, Data& data, std::mutex& dataMutex);

/**
 * Předspracoje data podle intervalu
 * @param fileStart Počáteční řádek pro zpracování
 * @param fileEnd Konečný řádek pro zpracování
 * @param filePath Cesta k souboru s daty
 * @param data Reference na objekt Data, do kterého budou přidány zpracované stanice
 * @param dataMutex Mutex pro synchronizaci přístupu k objektu Data
 */
void PreprocessByInterval(int fileStart, int fileEnd, std::filesystem::path filePath, Data& data, std::mutex& dataMutex, int threadIndex);

/**
 * Identifikuje odchylky v měsíčních průměrných teplotách mezi po sobě jdoucími roky pro danou stanici a zapisuje je do souboru.
 * @param station Reference na objekt Station, pro který budou odchylky identifikovány
 * @param deflectionFile Reference na ofstream pro zápis odchylek
 * @param deflectionFileMutex Mutex pro synchronizaci přístupu k výstupnímu souboru odchylek
 */
void deflectionIdentificationUtil(Station& station, std::ofstream& deflectionFile, std::mutex& deflectionFileMutex);

/**
 * Vykreslí SVG soubor pro daný měsíc a rok na základě dat o stanicích a jejich průměrných měsíčních teplotách.
 * @param year Rok pro který bude SVG vykreslen
 * @param month Měsíc pro který bude SVG vykreslen
 * @param data Reference na objekt Data obsahující informace o stanicích a jejich měsíčních průměrných teplotách
 * @param outDir Cesta k adresáři, kam bude SVG soubor uložen
 * @param svgTemplate Cesta k šabloně SVG souboru, která bude použita jako základ pro vykreslení
 */
void drawMonthSVG(int year, int month, Data& data, std::filesystem::path outDir, std::filesystem::path svgTemplate);

/**
 * Interpoluje barvu mezi modrou (pro minimum), žlutou (pro střed) a červenou (pro maximum) na základě zadaného procenta.
 * @param percentage Procento určující pozici mezi minimem a maximem (0.0 pro minimum, 0.5 pro střed, 1.0 pro maximum)
 * @return Interpolovaná barva v RGB formátu
 */
RGB interpolateColors(float percentage);

/**
 * Vypočítá maximální a minimální průměrné teploty pro daný měsíc a rok na základě dat o stanicích.
 * @param year Rok pro který budou extrémy vypočítány
 * @param month Měsíc pro který budou extrémy vypočítány
 * @param data Reference na objekt Data obsahující informace o stanicích a jejich měsíčních průměrných teplotách
 * @return Pár obsahující maximální a minimální průměrné teploty pro daný měsíc a rok. Pokud nejsou k dispozici žádná data, vrací pár s hodnotami NaN.
 */
std::pair<float, float> calculateMaxMinValues(int year, int month, const Data& data);

/**
 * Vrátí SVG element pro danou stanici na základě její zeměpisné šířky, délky, průměrné měsíční teploty a extrémů pro daný měsíc a rok.
 * @param latitude Zeměpisná šířka stanice
 * @param longitude Zeměpisná délka stanice
 * @param svgWidth Šířka SVG souboru
 * @param svgHeight Výška SVG souboru
 * @param monthAverage Průměrná teplota pro daný měsíc
 * @param maxValue Maximální průměrná teplota pro daný měsíc a rok
 * @param minValue Minimální průměrná teplota pro daný měsíc a rok
 */
std::string returnSVGDotprompt(float latitude, float longitude, float svgWidth, float svgHeight, float monthAverage, float maxValue, float minValue);