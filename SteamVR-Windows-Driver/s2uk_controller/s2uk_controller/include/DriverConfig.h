#pragma once
#ifndef S2UK_DriverConfig
#define S2UK_DriverConfig

#include <json.hpp>
#include <filesystem>
#include <cmath>

class DriverConfig {
public:
	struct configStruct {
		double headEMA = .3;
		double leftHandEMA = .3;
		double rightHandEMA = .3;

		int sensorTilt = 0;
	};

	DriverConfig() {
		cfgPath = getDllFilePath(fpFailed) / "driver_config.json";
		if (!configExists() && !fpFailed) {
			createConfig();
		}
		if(!readConfig_i(cfg)) readConfig_i(cfg);
	}

	bool configExists() { return std::filesystem::exists(cfgPath); }

	bool readConfig() { return readConfig_i(cfg); }

	bool createConfig() { return writeConfig(); }

	configStruct& getConfig() { return cfg; }
private:
	configStruct cfg;
	std::filesystem::path cfgPath;

	bool restoredConfig = false;

	bool readConfig_i(configStruct& out);

	bool writeConfig();

	bool fpFailed = false;
	std::filesystem::path getDllFilePath(bool& ok);
};
#endif