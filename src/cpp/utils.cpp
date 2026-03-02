#include"../hpp/utils.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <limits>
#include <fstream>
#include <filesystem>

long dateToDays(int y, int m, int d) {
	using namespace std::chrono;
	year_month_day ymd{year{y}, month{(unsigned)m}, day{(unsigned)d}};
	return sys_days{ymd}.time_since_epoch().count();
}

StationInfo::StationInfo(std::string name, float latitude, float longitude){
	this->name = name;
	this->latitude = latitude;
	this->longitude = longitude;
}

void Data::addStationInfo(int id, std::string name, float latitude, float longitude){
	this->stationsInfo.insert_or_assign(id, StationInfo(name, latitude, longitude));
}

void addStationsInfo(std::filesystem::path filePath, Data& data){
	std::ifstream inputFile(filePath);
	std::string line;

	// Kontrola otevření souboru
	if(!inputFile.is_open()) {
		std::cerr << "Failed to open file: " << filePath << std::endl;
		return;
	}

	// Přečteme hlavičku souboru a ignorujeme ji
	std::getline(inputFile, line);

	// Čteme soubor řádek po řádku
	while(std::getline(inputFile, line)) {
		std::stringstream ss(line);
		std::string cell;
		std::vector<std::string> row;

		// Rozdělíme řádek na jednotlivé buňky pomocí ';' jako oddělovače
		while (std::getline(ss, cell, ';')) {
			row.push_back(cell);
		}

		// Kontrola, zda řádek obsahuje dostatek dat (ID, název, šířka, délka)
		if (row.size() < 4) {
			continue;
		}

		// Získáme ID, název, zeměpisnou šířku a délku z řádku
		int id = std::stoi(row[0]);
		std::string name = row[1];
		float latitude = std::stof(row[2]);
		float longitude = std::stof(row[3]);

		// Přidáme informace o stanici do objektu Data
		data.addStationInfo(id, name, latitude, longitude);
	}
}

void addIfHasFiveContinuousYears(std::vector<std::string> lines, Data& data, std::mutex& dataMutex) {
	// Pokud nejsou žádné řádky, není co zpracovávat
	if (lines.empty()){
		return;
	}

	// Vytvoříme objekt Station pro zpracování data získání průměrů
	Station station;

	// Potřebné proměnné pro kontrolu pěti let kontinuálních dat a průměru 100 dní měření za rok
	bool hasFiveContinuousYears = false;
	bool hasAverage100DaysPerYear = false;
	int continuousDays = 1;
	int countingDays = 0;
	long previousTotalDays = -1;
	int yfirst = 0, mfirst = 0, dfirst = 0;
	int ylast = 0, mlast = 0, dlast = 0;

	// Iterace přes všechny řádky pro danou stanici a zpracování dat pro získání průměrů a kontrolu kontinuity dat
	for (int i = 0; i < lines.size(); ++i) {
		std::stringstream ss(lines[i]);
		std::string cell;
		std::vector<std::string> row;

		// Rozdělíme řádek na jednotlivé buňky pomocí ';' jako oddělovače
		while (std::getline(ss, cell, ';')) {
			row.push_back(cell);
		}

		// Kontrola, zda řádek obsahuje dostatek dat (ID, rok, měsíc, den, teplota)
		if (row.size() < 6) {
			continue;
		}

		// Získáme ID, rok, měsíc, den a teplotu z řádku
		int id = std::stoi(row[0]);
		int y = std::stoi(row[2]);
		int m = std::stoi(row[3]);
		int d = std::stoi(row[4]);
		double value = std::stod(row[5]);
		station.id = id;

		// Uložení data prvního měření pro výpočet průměru dní měření
		if(i == 0) {
			yfirst = y;
			mfirst = m;
			dfirst = d;
		}
		// Uložení data posledního měření pro výpočet průměru dní měření
		if(i == lines.size() - 1) {
			ylast = y;
			mlast = m;
			dlast = d;
		}

		// Přidáme denní měření do objektu Station a zvýšíme počet dní měření
		station.addDailyMeasurement(y, m, value);
		countingDays++;

		// Kontrola pěti let kontinuálních dat
		long currentTotalDays = dateToDays(y, m, d);

		// Porovnáme aktuální datum s předchozím datem a zjistíme, zda jsou data kontinuální (následující den) 
		if (previousTotalDays != -1) {
			if (currentTotalDays == previousTotalDays + 1) {
				continuousDays++;
			} 
			else {
				continuousDays = 1;
			}
		}

		// Pokud máme 1826 nebo více kontinuálních dní (což odpovídá přibližně pěti letům), nastavíme příznak pro pět let kontinuálních dat
		if (continuousDays >= 1826) {
			hasFiveContinuousYears = true;
		}

		previousTotalDays = currentTotalDays;
	}

	// Výpočet průměru dní měření za rok a kontrola, zda je průměr alespoň 100 dní měření za rok
	int calendarDays = dateToDays(ylast, mlast, dlast) - dateToDays(yfirst, mfirst, dfirst) + 1;
	double totalYears = static_cast<double>(calendarDays) / 365.25;
	double averageDaysPerYear = totalYears > 0.0
		? static_cast<double>(countingDays) / totalYears
		: 0.0;
	
	// Pokud je průměr dní měření za rok alespoň 100, nastavíme příznak pro průměr 100 dní měření za rok
	if (averageDaysPerYear >= 100.0) {
		hasAverage100DaysPerYear = true;
	}

	// Pokud máme pět let kontinuálních dat a průměr alespoň 100 dní měření za rok, přidáme stanici do objektu Data a vypočítáme měsíční průměry
	if(hasFiveContinuousYears && hasAverage100DaysPerYear) {
		std::lock_guard<std::mutex> lock(dataMutex);
		station.calculateMonthlyAverages();
		data.stations.push_back(station);
	}
}

// Vrací ID stanice podle řádky
int returnId(std::string line){
	std::stringstream ss(line);
	std::string cell;
	std::vector<std::string> row;

	while (std::getline(ss, cell, ';')) {
		row.push_back(cell);
	}
	try{
		int id = std::stoi(row[0]);
		return id;
	} catch (const std::exception& e) {
		std::cerr << "Error parsing ID from line: " << line << " - " << e.what() << std::endl;
		return -1; 
	}
}
// Odstaní nám na intervalu řádka s první stanicí a nenačítá žádné řádky s první stanicí, pokud je první řádek intervalu začátkem souboru. Na konci intervalu nám načte všechny řádky s poslední stanicí, aby nám neoddělil stanici mezi dvěma intervaly.
// A bude načítat poslední řádky s poslední stanicí, aby nám neoddělil stanici mezi dvěma intervaly.
std::vector<std::string> clearInterval(int fileStart, int fileEnd, std::filesystem::path filePath){
	std::vector<std::string> lines;
	
	FILE* file = fopen(filePath.string().c_str(), "r");
	if (file == nullptr) {
		std::cerr << "Failed to open file: " << filePath << std::endl;
		return std::vector<std::string>();
	}

	// Nastavíme pozici souboru na začátek intervalu a přeskočíme hlavičku, pokud jsme na začátku souboru
	fseek(file, fileStart, SEEK_SET);
	char buffer[1024];
	if(fileStart == 0) {
		if (fgets(buffer, sizeof(buffer), file) == nullptr) {
			fclose(file);
			return std::vector<std::string>();
		}
	}

	// Zjisteme ID první stanice v našem intervalu, abychom ji mohli přeskočit
	// První nezpracujeme a poslední zprácujeme až do konce intervalu, abychom zajistili, že nebudeme mít rozdělenou stanici mezi dvěma intervaly
	if (fgets(buffer, sizeof(buffer), file) == nullptr) {
		fclose(file);
		return std::vector<std::string>();
	}
	std::string line(buffer);
	int firstId = returnId(line);
	int skipId = (fileStart == 0) ? -1 : firstId;
	std::string lastLine;

	if (fileStart == 0 && firstId != -1) {
		lastLine = line;
		lines.push_back(line);
	}
	
	// Načítá řádky do té doby než narazíme na konec našeho intervalu
	while (ftell(file) < fileEnd) {
		char buffer[1024];
		if (fgets(buffer, sizeof(buffer), file) == nullptr) {
			break;
		}
		std::string line(buffer);
		
		int id = returnId(line);
		if (line.empty() || id == -1 || id == skipId){
			continue;
		}
		lastLine = line;
		lines.push_back(line);
	}

	if (lastLine.empty()) {
		fclose(file);
		return std::vector<std::string>();
	}

	int lastId = returnId(lastLine);
	if (lastId == -1) {
		fclose(file);
		return std::vector<std::string>();
	}
	
	// Načteme všechny řádky s poslední stanicí, abychom zajistili, že nebudeme mít rozdělenou stanici mezi dvěma intervaly
	while (true) {
		char buffer[1024];
		if (fgets(buffer, sizeof(buffer), file) == nullptr) {
			break;
		}
		std::string line(buffer);
		
		int id = returnId(line);
		if (line.empty() || id == -1 || id != lastId){
			break;
		}
		lines.push_back(line);
	}
	fclose(file);
	return lines;
}

void PreprocessByInterval(int fileStart, int fileEnd, std::filesystem::path filePath, Data& data, std::mutex& dataMutex, int threadIndex) {
	// Timto se nam vycisti interval a mame jen stanice ktere potrebujeme zpracovávat
	std::vector<std::string> lines = clearInterval(fileStart, fileEnd, filePath);
	std::vector<std::string> stationLines;
	int currentId = -1;

	// Každou řádku pak přiřazujeme ke každe stanici a následne je posíláme na dalsi zpracování
	for (const auto& stationLine : lines) {
		// Uz zde hledáme ID a zároven extremy ze vsech namerených dat
		int id = returnId(stationLine);
		findExtremes(stationLine, data, threadIndex);

		if (id == -1) {
			continue;
		}

		if (currentId == -1) {
			currentId = id;
		}

		// Pokud jsem dorazili na radku s novou stanici, dosavadní posílámedo dalsiho zpracovani
		if (id != currentId) {
			if (!stationLines.empty()) {
				addIfHasFiveContinuousYears(stationLines, data, dataMutex);
			}
			stationLines.clear();
			currentId = id;
		}

		stationLines.push_back(stationLine);
	}

	// Pokud je stanice v intervalu poslední
	if (!stationLines.empty()) {
		addIfHasFiveContinuousYears(stationLines, data, dataMutex);
	}
}

void findExtremes(std::string line, Data& data, int threadIndex){
	std::stringstream ss(line);
	std::string cell;
	std::vector<std::string> row;

	while (std::getline(ss, cell, ';')) {
		row.push_back(cell);
	}

	// Kazdemu vlaknu prislusí jeden index v poli do ktereho zapisuje lokální extremy
	// Ty jsou pak vyhodnoceny jako globální 
	try{
		float value = std::stof(row[5]);
		if(data.localMaxs[threadIndex] < value){
			data.localMaxs[threadIndex] = value;
		}
		if(data.localMins[threadIndex] > value){
			data.localMins[threadIndex] = value;
		}
	} 
	catch (const std::exception& e) {
		std::cerr << "Error parsing value from line: " << line << " - " << e.what() << std::endl;
		return;
	}
}

void printExtremes(const float monthlyMax[12], const float monthlyMin[12], const bool hasData[12]){
	for (int i = 0; i < 12; ++i) {
		if (!hasData[i]) {
			std::cout << "Month " << (i + 1) << ": no data" << std::endl;
			continue;
		}

		float deflection = monthlyMax[i] - monthlyMin[i];
	}
}

void deflectionIdentificationUtil(Station& station, std::ofstream& deflectionFile, std::mutex& deflectionFileMutex) {
	float monthlyMax[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
	float monthlyMin[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
	bool hasData[12] = {false, false, false, false, false, false, false, false, false, false, false, false};
	
	deflectionFileMutex.lock();
	deflectionFile << "Station ID;Month;Year;Deflection" << std::endl;
	deflectionFileMutex.unlock();
	// Iterrace pres každý rok
	for(const auto& yearEntry : station.monthlyData){
		// Iterace přes každý měsíc
		for(const auto& monthEntry : yearEntry.second){
			int month = monthEntry.first - 1;
			if (month < 0 || month >= 12) {
				continue;
			}
			double averageTemp = monthEntry.second.first;

			// HasData je zde pouzito aby nedochazelo k nevaidnimu porovnani s napriklad u minim s nulama
			if (!hasData[month]) {
				hasData[month] = true;
				monthlyMax[month] = static_cast<float>(averageTemp);
				monthlyMin[month] = static_cast<float>(averageTemp);
			} 
			else {
				// Pokud jsou extremy nalezeny jsou validne zapsany
				if(averageTemp > monthlyMax[month]) {
					monthlyMax[month] = static_cast<float>(averageTemp);
				}
				if(averageTemp < monthlyMin[month]) {
					monthlyMin[month] = static_cast<float>(averageTemp);
				}
			}
		}
	}

	auto it = station.monthlyData.begin();
	if (it == station.monthlyData.end()) {
		return;
	}
	auto nextIt = std::next(it);
	if (nextIt == station.monthlyData.end()) {
		return;
	}

	// Hledani odchylek mezi dvema roky, proto jsou zde pouzity dva iteratory, jeden pro rok n, druhy pro n + 1
	while(nextIt != station.monthlyData.end()) {
		auto& currentYearData = it->second;
		auto& nextYearData = nextIt->second;
		
		for(int month = 1; month <= 12; month++){
			// Iteratory pres mesice
			auto currentMonthIt = currentYearData.find(month);
			auto nextMonthIt = nextYearData.find(month);
			if (currentMonthIt == currentYearData.end() || nextMonthIt == nextYearData.end()) {
				continue;
			}
			if (!hasData[month - 1]) {
				continue;
			}

			// Rozdil po sobe jdoucich mesicu
			float cyd = static_cast<float>(currentMonthIt->second.first);
			float nyd = static_cast<float>(nextMonthIt->second.first);
			float diff = std::abs(cyd - nyd);
			float avgMax = monthlyMax[month - 1];
			float avgMin = monthlyMin[month - 1];
			
			// Vypocet odchylky a zapis do predaneho csv
			//deflectionFile << "Station ID;Month;Year;Deflection" << std::endl;
			if(diff > (0.75 * (avgMax - avgMin))){
				deflectionFileMutex.lock();
				deflectionFile << station.id << ";" << month << ";" << nextIt->first << ";" << diff << std::endl;
				deflectionFileMutex.unlock();
			}
		}

		++it;
		++nextIt;
	}
}

void drawMonthSVG(int year, int month, Data& data, std::filesystem::path outDir, std::filesystem::path svgTemplate){
	std::pair<double, double> extremes = std::pair<double, double>(data.globalMax, data.globalMin);
	std::pair<float, float> monthExtremes = calculateMaxMinValues(year, month, data);
	std::string filename = MonthNameLookup::getMonthName(month);
	std::ofstream svgFile(outDir / (filename + ".svg"));
	
	// Kontrola cesty k vyslednemu svg
	if(!svgFile.is_open()) {
		std::cerr << "Failed to create SVG file: " << (outDir / (filename + "_" + std::to_string(year) + ".svg")) << std::endl;
		return;
	}
	// Konstrola cesty k templatu
	if(!std::filesystem::exists(svgTemplate)) {
		std::cerr << "SVG template not found: " << svgTemplate << std::endl;
		return;
	}

	// Vytvoreni vystupního svg
	std::ifstream templateFile(svgTemplate);
	if (!templateFile.is_open()) {
		std::cerr << "Failed to open SVG template: " << svgTemplate << std::endl;
		return;
	}

	std::stringstream templateBuffer;
	templateBuffer << templateFile.rdbuf();
	std::string templateContent = templateBuffer.str();

	// Kontrola validity svg templatu
	std::size_t svgEnd = templateContent.rfind("</svg>");
	if (svgEnd == std::string::npos) {
		std::cerr << "Invalid SVG template format: " << svgTemplate << std::endl;
		return;
	}

	float svgWidth = 1000.0f;
	float svgHeight = 1000.0f;
	std::size_t svgTagStart = templateContent.find("<svg");
	if (svgTagStart != std::string::npos) {
		std::size_t svgTagEnd = templateContent.find('>', svgTagStart);
		if (svgTagEnd != std::string::npos) {
			std::string svgTag = templateContent.substr(svgTagStart, svgTagEnd - svgTagStart + 1);
			std::size_t viewBoxPos = svgTag.find("viewBox=\"");
			if (viewBoxPos != std::string::npos) {
				viewBoxPos += 9;
				std::size_t viewBoxEnd = svgTag.find('"', viewBoxPos);
				if (viewBoxEnd != std::string::npos) {
					std::string viewBoxValue = svgTag.substr(viewBoxPos, viewBoxEnd - viewBoxPos);
					float minX = 0.0f;
					float minY = 0.0f;
					if (std::sscanf(viewBoxValue.c_str(), "%f %f %f %f", &minX, &minY, &svgWidth, &svgHeight) != 4) {
						svgWidth = 1000.0f;
						svgHeight = 1000.0f;
					}
				}
			}
		}
	}

	// Do overlay zapisujeme nase body na mape a ten je pak zapsan do vysledneho streamu
	std::stringstream overlay;
	float monthAverage;
	int stationID;

	// Iterumene pres kazdou stanici, kterou mame ulozenou a zjistujeme jeji parametry
	// Ty pak nasledne posleme do pomcne funkce, ktera nam vygeneruje kod pro svg na konkretni stanici
	for(const auto& station : data.stations){
		stationID = station.id;
		auto yIt = station.monthlyData.find(year);
		
		if(yIt == station.monthlyData.end()){
			continue;
		}

		auto monthData = yIt->second;
		auto mIt = monthData.find(month);
		if(mIt == monthData.end()){
			continue;
		}
		monthAverage = mIt->second.first;
		auto stationInfoIt = data.stationsInfo.find(stationID);
		if (stationInfoIt == data.stationsInfo.end()) {
			continue;
		}
		float lat = stationInfoIt->second.latitude;
		float lon = stationInfoIt->second.longitude;
		overlay << returnSVGDotprompt(lat, lon, svgWidth, svgHeight, monthAverage, extremes.first, extremes.second);
	}

	float legendMin = std::isfinite(monthExtremes.second) ? monthExtremes.second : static_cast<float>(extremes.second);
	float legendMax = std::isfinite(monthExtremes.first) ? monthExtremes.first : static_cast<float>(extremes.first);
	float legendX = svgWidth * 0.02f;
	float legendY = svgHeight * 0.03f;
	float legendWidth = svgWidth * 0.30f;
	float legendPaddingX = 12.0f;
	float legendFirstLineY = 22.0f;
	float legendLineStep = 18.0f;
	float legendHeight = 108.0f;

	std::stringstream legend;
	legend << "<g id=\"legend\">\n";
	legend << "<rect x=\"" << legendX
		<< "\" y=\"" << legendY
		<< "\" width=\"" << legendWidth
		<< "\" height=\"" << legendHeight
		<< "\" fill=\"white\" fill-opacity=\"0.85\" stroke=\"black\" stroke-width=\"1\" />\n";
	legend << "<text x=\"" << (legendX + legendPaddingX)
		<< "\" y=\"" << (legendY + legendFirstLineY)
		<< "\" font-size=\"16\" font-family=\"Arial\" fill=\"black\">"
		<< "Mesic: " << MonthNameLookup::getMonthName(month) << "  Rok: " << year
		<< "</text>\n";
	legend << "<text x=\"" << (legendX + legendPaddingX)
		<< "\" y=\"" << (legendY + legendFirstLineY + legendLineStep)
		<< "\" font-size=\"15\" font-family=\"Arial\" fill=\"black\">"
		<< "Min teplota: " << legendMin << " C"
		<< "</text>\n";
	legend << "<text x=\"" << (legendX + legendPaddingX)
		<< "\" y=\"" << (legendY + legendFirstLineY + 2.0f * legendLineStep)
		<< "\" font-size=\"15\" font-family=\"Arial\" fill=\"black\">"
		<< "Max teplota: " << legendMax << " C"
		<< "</text>\n";
	legend << "<text x=\"" << (legendX + legendPaddingX)
		<< "\" y=\"" << (legendY + legendFirstLineY + 3.0f * legendLineStep)
		<< "\" font-size=\"15\" font-family=\"Arial\" fill=\"black\">"
		<< "Global min (data): " << extremes.second << " C"
		<< "</text>\n";
	legend << "<text x=\"" << (legendX + legendPaddingX)
		<< "\" y=\"" << (legendY + legendFirstLineY + 4.0f * legendLineStep)
		<< "\" font-size=\"15\" font-family=\"Arial\" fill=\"black\">"
		<< "Global max (data): " << extremes.first << " C"
		<< "</text>\n";
	legend << "</g>\n";

	// tabulka pro globalni min a max + konkretni mesic (nazev v cestine) a rok
	svgFile << templateContent.substr(0, svgEnd);
	svgFile << "<!-- Global Max: " << extremes.first << " C, Global Min: " << extremes.second << " C, Month: " << MonthNameLookup::getMonthName(month) << ", Year: " << year << " -->\n";
	svgFile << legend.str();
	svgFile << overlay.str();
	svgFile << "</svg>\n";

	svgFile.close();
}

RGB interpolateColors(float percentage){
	// blue je pro minimum, red pro maximum a žlutá pro střed
	RGB red = RGB(255, 0, 0);
	RGB yellow = RGB(255, 255, 0);
	RGB blue = RGB(0, 0, 255);
	
	if(percentage > 1 || percentage < 0){
		std::cout << "Chybná RGB, percentage=" << percentage << std::endl;
		return RGB(0, 0, 0);
	}
	// Pokud je to presne 0.5 vracime zlutou
	if(percentage == 0.5){
		return yellow;
	}

	// Pokud je to mezi 0.5 a 1 vracime interpolaci mezi red a yellow
	if(percentage > 0.5){
		float ratio = (percentage - 0.5f) / 0.5f;
		return RGB(
			static_cast<int>(yellow.r + ratio * (red.r - yellow.r)),
			static_cast<int>(yellow.g + ratio * (red.g - yellow.g)),
			static_cast<int>(yellow.b + ratio * (red.b - yellow.b))
		);
	}
	// V opacnem pripade vracime interpolaci mezi blue a yellow
	else{
		float ratio = percentage / 0.5f;
		return RGB(
			static_cast<int>(blue.r + ratio * (yellow.r - blue.r)),
			static_cast<int>(blue.g + ratio * (yellow.g - blue.g)),
			static_cast<int>(blue.b + ratio * (yellow.b - blue.b))
		);
	}
}


std::string returnSVGDotprompt(float latitude, float longitude, float svgWidth, float svgHeight, float monthAverage, float maxValue, float minValue){
	std::stringstream ss;
	// Konstanty pro převod zeměpisných souřadnic na souřadnice v SVG, které jsou založeny na rozsahu České republiky, aby se nám body správně zobrazily na mapě
	const float topLeftLat = 51.03806105663445f;
	const float topLeftLon = 12.102209054269062f;
	const float bottomRightLat = 48.521003814763994f;
	const float bottomRightLon = 18.866923511078615f;

	// Vypočítáme procentuální pozici měsíčního průměru mezi globálním minimem a maximem, abychom mohli určit barvu bodu na mapě
	float denominator = (maxValue - minValue);
	float percentage = (monthAverage - minValue) / denominator;
	RGB color = interpolateColors(percentage);

	const float latitudeScale = topLeftLat - bottomRightLat;
	const float longitudeScale = bottomRightLon - topLeftLon;

	float scaledX = (longitude - topLeftLon) / longitudeScale * svgWidth;
	float scaledY = (topLeftLat - latitude) / latitudeScale * svgHeight;

	ss << "<circle cx=\"" << scaledX << "\" cy=\"" << scaledY << "\" r=\"5\" fill=\"rgb(" << color.r << "," << color.g << "," << color.b << ")\" />\n";
	return ss.str();
}

std::pair<float, float> calculateMaxMinValues(int year, int month, const Data& data){
	float maxValue = std::numeric_limits<float>::lowest();
	float minValue = std::numeric_limits<float>::max();
	bool found = false;

	for(const auto& station : data.stations){
		
		auto yIt =  station.monthlyData.find(year);
		if(yIt == station.monthlyData.end()){
			continue;
		}

		const auto& monthData = yIt->second;
		auto mIt = monthData.find(month);
		if(mIt == monthData.end()){
			continue;
		}

		maxValue = std::max(maxValue, static_cast<float>(mIt->second.first));
		minValue = std::min(minValue, static_cast<float>(mIt->second.first));
		found = true;
	}
	return found ? std::pair<float, float>(maxValue, minValue) : std::pair<float, float>(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());
}


