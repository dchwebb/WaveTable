#pragma once

#include "initialisation.h"
#include <vector>

// Struct added to classes that need settings saved
struct ConfigSaver {
	void* settingsAddress;
	uint32_t settingsSize;
	void (*validateSettings)(void);		// function pointer to method that will validate config settings when restored
};


class Config {
public:
	static constexpr uint8_t configVersion = 5;
	
	// STM32H7B0 has 128k Flash in 16 sectors of 8192k
	static constexpr uint32_t flashConfigSector = 14;		// Allow 3 sectors for config giving a config size of 24k before erase needed
	static constexpr uint32_t flashSectorSize = 8192;
	static constexpr uint32_t configSectorCount = 3;		// Number of sectors after base sector used for config
	uint32_t* flashConfigAddr = reinterpret_cast<uint32_t* const>(FLASH_BASE + flashSectorSize * (flashConfigSector - 1));;

	bool scheduleSave = false;
	uint32_t saveBooked = false;

	// Constructor taking multiple config savers: Get total config block size from each saver
	Config(std::initializer_list<ConfigSaver*> initList) : configSavers(initList) {
		for (auto saver : configSavers) {
			settingsSize += saver->settingsSize;
		}
		// Ensure config size (+ 4 byte header + 1 byte index) is aligned to 8 byte boundary
		settingsSize = AlignTo16Bytes(settingsSize + headerSize);
	}

	void ScheduleSave();				// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
	bool SaveConfig(bool forceSave = false);
	void EraseConfig();					// Erase flash page containing config
	void RestoreConfig();				// gets config from Flash, checks and updates settings accordingly

private:
	static constexpr uint32_t flashAllErrors = FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_STRBERR | FLASH_SR_INCERR | FLASH_SR_RDPERR | FLASH_SR_RDSERR | FLASH_SR_SNECCERR | FLASH_SR_DBECCERR | FLASH_SR_CRCRDERR;

	const std::vector<ConfigSaver*> configSavers;
	uint32_t settingsSize = 0;			// Size of all settings from each config saver module + size of config header

	const char ConfigHeader[4] = {'C', 'F', 'G', configVersion};
	static constexpr uint32_t headerSize = sizeof(ConfigHeader) + 1;
	int32_t currentSettingsOffset = -1;	// Offset within flash page to block containing active/latest settings

	uint32_t currentIndex = 0;			// Each config gets a new index to track across multiple sectors
	uint32_t currentSector = flashConfigSector;			// Sector containing current config
	struct CfgSector {
		uint32_t sector;
		uint8_t index;
		bool dirty;
	} sectors[configSectorCount];

	void SetCurrentConfigAddr() {
		flashConfigAddr = reinterpret_cast<uint32_t* const>(FLASH_BASE + flashSectorSize * (currentSector - 1));
	}
	void FlashUnlock();
	void FlashLock();
	void FlashEraseSector(uint8_t Sector);
	bool FlashWaitForLastOperation();
	bool FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size);

	static const inline uint32_t AlignTo16Bytes(uint32_t val) {
		val += 15;
		val >>= 4;
		val <<= 4;
		return val;
	}
};

extern Config config;
