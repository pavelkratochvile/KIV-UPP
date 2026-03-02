#include<iostream>
#include<filesystem>
#include"../hpp/utils.hpp"
/**
 * Třída Serial představuje sériovou verzi zpracování dat. Obsahuje metody pro předzpracování dat, identifikaci výkyvů, přidání informací o stanicích z souboru, nalezení globálních extrémů a generování SVG grafů pro jednotlivé měsíce.
 */
class Serial{
public:
    /**
     * Cesta k souboru s daty, která bude zpracována.
     */
    std::filesystem::path filePath;
    /**
     * Objekt Data, který slouží jako kontejner pro globální a lokální extrémy, seznam stanic a mapu informací o stanicích.
     */
    Data data;
    /**
     * Mutex pro synchronizaci přístupu k objektu Data, i když v sériové verzi není potřeba, je zde pro konzistenci s paralelní verzí.
     */
    std::mutex dataMutex;
    /**
     * Konstruktor třídy Serial, který inicializuje cestu k souboru s daty.
     * @param filePath Cesta k souboru s daty, která bude z
     */
    Serial(std::filesystem::path filePath) : filePath(filePath) {}
    /**
     * Metoda Preprocess provádí předzpracování dat. V této metodě se načítají data ze souboru, zpracovávají se a ukládají do objektu Data.
     */
    int Preprocess();
    /**
     * Metoda deflectionIdentification identifikuje odchylky v měsíčních průměrných teplotách mezi po sobě jdoucími roky pro danou stanici a zapisuje je do souboru.
     * @param outputDir Cesta k adresáři, kam bude uložen soubor
     */
    int deflectionIdentification(std::filesystem::path outputDir);
    /**
     * Metoda addStationsInfoFromFile přidává informace o meteorologických stanicích ze souboru do objektu Data.
     * @param filePath Cesta k souboru s informacemi o stanicích.
     */
    int addStationsInfoFromFile(std::filesystem::path filePath);
    /**
     * Metoda findGlobalExtremes nalezne globální extrémy pro všechny stanice a uloží je do objektu Data.
     */
    int findGlobalExtremes();
    /**
     * Metoda drawSVGs vykreslí SVG soubor pro každý měsíc a rok na základě dat o stanicích a jejich průměrných měsíčních teplotách.
     * @param outDir Cesta k adresáři, kam budou SVG soubory uloženy
     * @param svgTemplate Cesta k šabloně SVG souboru, která bude použita jako základ pro vykreslení
     * @param year Rok pro který budou SVG soubory vykresleny
     */
    int drawSVGs(std::filesystem::path outDir, std::filesystem::path svgTemplate, int year);
};
