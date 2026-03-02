#include"../hpp/parallel.hpp"
#include"../hpp/serial.hpp"
#include<chrono>
#include<iostream>

int main(int argc, char* argv[]) {
    if(argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <stations_info_file> <measurements_file> <--serial or --parallel>" << std::endl;
        return 1;
    }
    
    bool isParallel = false;
    if (std::string(argv[3]) == "--parallel") {
        isParallel = true;
    } else if (std::string(argv[3]) != "--serial") {
        std::cerr << "Invalid argument. Use either --serial or --parallel." << std::endl;
        return 1;
    }

    if(!std::filesystem::exists(argv[1])){
        std::cerr << "Stations info file not found: " << argv[1] << std::endl;
        return 1;
    }
    if(!std::filesystem::exists(argv[2])){
        std::cerr << "Measurements file not found: " << argv[2] << std::endl;
        return 1;
    }

    // Cesty k souborům a adresářům pro vstupní data, výstupní data a šablonu SVG pro vykreslení grafů. Tyto cesty jsou nastaveny relativně k umístění spustitelného souboru, takže je důležité zajistit, aby struktura adresářů odpovídala těmto cestám, aby program správně načítal vstupní data a ukládal výstupy.
    std::filesystem::path outputPath("../output");
    std::filesystem::path mapPath("../data/czmap.svg");
    std::filesystem::path dataMeasurePath(argv[2]);
    std::filesystem::path dataStationPath(argv[1]);
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;
    std::chrono::steady_clock::time_point startTotal;
    std::chrono::steady_clock::time_point endTotal;
    long long tpParallelMs = 0;
    int i;
    
    // ----------------------------------------------------------------------------------
    //Serial
    if(!isParallel){
        startTotal = std::chrono::steady_clock::now();

        std::cout << "Starting Serial Version..." << std::endl;
        Serial s = Serial(dataMeasurePath);
        
        start = std::chrono::steady_clock::now();
        i = s.Preprocess();
        if (i != 0) { std::cerr << "Serial::Preprocess selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        std::cout << "Preprocessing time in serial version: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
        
        start = std::chrono::steady_clock::now();
        i = s.addStationsInfoFromFile(dataStationPath);
        if (i != 0) { std::cerr << "Serial::addStationsInfoFromFile selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        std::cout << "Adding station info time in serial version: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
        
        start = std::chrono::steady_clock::now();
        i = s.findGlobalExtremes();
        if (i != 0) { std::cerr << "Serial::findGlobalExtremes selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        std::cout << "Finding global extremes time in serial version: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
        
        start = std::chrono::steady_clock::now();
        i = s.deflectionIdentification(outputPath);
        if (i != 0) { std::cerr << "Serial::deflectionIdentification selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        std::cout << "Deflection identification time in serial version: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;

        start = std::chrono::steady_clock::now();
        i = s.drawSVGs(outputPath, mapPath, 2000);
        if (i != 0) { std::cerr << "Serial::drawSVGs selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        std::cout << "Drawing SVGs time in serial version: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
        
        endTotal = std::chrono::steady_clock::now();
        std::cout << "Total time in serial version: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTotal - startTotal).count() << " ms" << std::endl;
        std::cout << "Serial Version Completed." << std::endl;
    }

    
    if(isParallel){
        startTotal = std::chrono::steady_clock::now();

        std::cout << "Starting Parallel Version..." << std::endl;
        Parallel p = Parallel(dataMeasurePath, 10);
        
        start = std::chrono::steady_clock::now();
        i = p.Preprocess();
        if (i != 0) { std::cerr << "Parallel::Preprocess selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        long long preprocessParallelMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        tpParallelMs += preprocessParallelMs;
        std::cout << "Preprocessing time in parallel version: " << preprocessParallelMs << " ms" << std::endl;
        
        start = std::chrono::steady_clock::now();
        i = p.addStationsInfoFromFile(dataStationPath);
        if (i != 0) { std::cerr << "Parallel::addStationsInfoFromFile selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        std::cout << "Adding station info time in parallel version: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
        
        start = std::chrono::steady_clock::now();
        i = p.findGlobalExtremes();
        if (i != 0) { std::cerr << "Parallel::findGlobalExtremes selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        std::cout << "Finding global extremes time in parallel version: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
        
        start = std::chrono::steady_clock::now();
        i = p.deflectionIdentification(outputPath);
        if (i != 0) { std::cerr << "Parallel::deflectionIdentification selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        long long deflectionParallelMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        tpParallelMs += deflectionParallelMs;
        std::cout << "Deflection identification time in parallel version: " << deflectionParallelMs << " ms" << std::endl;
        
        start = std :: chrono :: steady_clock :: now();
        i = p.drawSVGs(outputPath, mapPath, 2000);
        if (i != 0) { std::cerr << "Parallel::drawSVGs selhalo" << std::endl; return i; }
        end = std::chrono::steady_clock::now();
        long long drawParallelMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        tpParallelMs += drawParallelMs;
        std::cout << "Drawing SVGs time in parallel version: " << drawParallelMs << " ms" << std::endl;
        
        endTotal = std::chrono::steady_clock::now();
        std::cout << "Total time in parallel version: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTotal - startTotal).count() << " ms" << std::endl;
        std::cout << "Parallel Version Completed." << std::endl;
        return 0;
    }
}