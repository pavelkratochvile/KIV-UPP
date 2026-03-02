#include<iostream>
#include<thread>
#include"../hpp/utils.hpp"
#include<mutex>
#include<filesystem>

class Parallel{
public:
    /**
     * Cesta k souboru s daty, která bude zpracována.
    */
    std::filesystem::path filePath;
    /**
     * Index pro sledování, která stanice je právě zpracovávána v rámci identifikace výkyvů. Tento index je sdílen mezi vlákny a je chráněn mutexem stationIndexMutex, aby se zabránilo současnému přístupu více vláken k této proměnné.
     */
    int stationTakingIndex;
    /**
     * Mutex pro synchronizaci přístupu k proměnné stationTakingIndex, která sleduje, která stanice je právě zpracovávána v rámci identifikace výkyvů. Tento mutex zajišťuje, že pouze jedno vlákno může současně aktualizovat tento index, což zabraňuje konfliktům a zajišťuje správné přiřazení stanic k vláknům.
     */
    std::mutex stationIndexMutex;
    /**
     * Mutex pro synchronizaci přístupu k souboru s výsledky identifikace výkyvů. Tento mutex zajišťuje, že pouze jedno vlákno může současně zapisovat do souboru, což zabraňuje konfliktům a zajišťuje správné ukládání výsledků.
     */
    std::mutex deflectionFileMutex;
    /**
     * Pocet vláken, která budou použita pro zpracování dat. Tento počet je určen při vytváření instance třídy Parallel a určuje, kolik vláken bude spuštěno pro zpracování dat a identifikaci výkyvů.
     */
    int numThreads;
    /**
     * Objekt Data, který slouží jako kontejner pro globální a lokální extrémy, seznam stanic a mapu informací o stanicích. Tento objekt je sdílen mezi vlákny a obsahuje všechny zpracované informace o stanicích, které jsou potřebné pro identifikaci výkyvů a generování SVG grafů.
     */
    Data data;
    /**
     * Vektor pro ukládání výsledků identifikace výkyvů. Tento vektor může být použit pro dočasné uchovávání výsledků, které jsou následně zapisovány do souboru s výsledky identifikace výkyvů. Přístup k tomuto vektoru by měl být synchronizován pomocí mutexu deflectionFileMutex, pokud je používán pro ukládání výsledků z více vláken.
     */
    std::vector<std::string> deflectionResults;
    /**
     * Mutex pro synchronizaci přístupu k objektu Data, který je sdílen mezi vlákny a obsahuje všechny zpracované informace o stanicích. Tento mutex zajišťuje, že pouze jedno vlákno může současně aktualizovat tento objekt, což zabraňuje konfliktům a zajišťuje správné ukládání informací o stanicích.
     */
    std::mutex dataMutex;
    /**
     * Konstruktor třídy Parallel, který inicializuje cestu k souboru s daty a počet vláken pro zpracování. Tento konstruktor také inicializuje index pro sledování zpracovávaných stanic a připravuje objekt Data pro ukládání zpracovaných informací o stanicích.
     * @param filePath Cesta k souboru s daty, která bude zpracována.
     * @param numThreads Počet vláken, která budou použita pro zpracování dat.
     */
    Parallel(std::filesystem::path filePath, int numThreads) : filePath(filePath), numThreads(numThreads) {}
    /**
     * Metoda Preprocess provádí předzpracování dat. V této metodě se načítají data ze souboru, zpracovávají se a ukládají do objektu Data. Předzpracování je rozděleno do více vláken, která zpracovávají různé části souboru, aby se zrychlil proces načítání a zpracování dat.
     */
    int Preprocess();
    /**
     * Metoda findGlobalExtremes nalezne globální extrémy pro všechny stanice a uloží je do objektu Data. Tato metoda by měla být volána po dokončení předzpracování dat, aby se zajistilo, že všechny informace o stanicích jsou k dispozici pro výpočet globálních extrémů.
     */
    int findGlobalExtremes();
    /**
     * Metoda deflectionIdentification identifikuje odchylky v měsíčních průměrných teplotách mezi po sobě jdoucími roky pro danou stanici a zapisuje je do souboru. Tato metoda využívá více vláken pro zpracování různých stanic současně, přičemž synchronizuje přístup k souboru s výsledky pomocí mutexu deflectionFileMutex, aby se zabránilo konfliktům při zápisu výsledků.
     * @param outputDir Cesta k adresáři, kam bude uložen soubor s výsledky identifikace výkyvů.
     */
    int deflectionIdentification(std::filesystem::path outputDir);
    /**
     * Metoda drawSVGs vykreslí SVG soubor pro každý měsíc a rok na základě dat o stanicích a jejich průměrných měsíčních teplotách. Tato metoda využívá více vláken pro zpracování různých měsíců současně, přičemž synchronizuje přístup k objektu Data pomocí mutexu dataMutex, aby se zabránilo konfliktům při čtení informací o stanicích.
     * @param outDir Cesta k adresáři, kam budou SVG soubory uloženy.
     * @param svgTemplate Cesta k šabloně SVG souboru, která bude použita jako základ pro vykreslení.
     * @param year Rok pro který budou SVG soubory vykresleny.
     */
    int drawSVGs(std::filesystem::path outDir, std::filesystem::path svgTemplate, int year);
    /**
     * Metoda addStationsInfoFromFile přidává informace o meteorologických stanicích ze souboru do objektu Data. Tato metoda načítá informace o stanicích z daného souboru a aktualizuje objekt Data, který je sdílen mezi vlákny, aby obsahoval všechny potřebné informace o stanicích pro další zpracování.
     */
    int addStationsInfoFromFile(std::filesystem::path filePath);
};