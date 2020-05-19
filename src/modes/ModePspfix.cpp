#include <iostream>
#include <fstream>
#include "ModePspfix.h"
#include "../definitions/PSPMapper.h"
#include "../Fs.h"
#include "../StickDownloader.h"
#include "../UserFolders.h"
#include "../StickExtractor.h"

ModePspfix::ModePspfix(std::string &targetDir) : targetDir(targetDir) {

}

bool ModePspfix::checkStockPath() {
    std::string path = getStockPath();
    if (!Fs::exists(path)) {
        std::cout << "Please run `pandory pspfix stage1` first to install the PSP injector via USB stick on to your"
                  << std::endl
                  << "Pandora Games 3D console and then take out and insert your SD card into drive " << targetDir
                  << std::endl
                  << "in order to fix stock games."
                  << std::endl;
        return false;
    }
    return true;
}

int ModePspfix::patchControlFolder(const std::string &source, const std::string &target, pspConfigGameDef gameDef) {
    Fs::makeDirectory(target + "PSP");
    Fs::makeDirectory(target + "PSP/SYSTEM");
    Fs::copy(source + "controls" + std::to_string(gameDef.controlType) + ".ini", target + "PSP/SYSTEM/controls.ini");
#ifdef NO_SHAREWARE_LIMIT
    Fs::copy(source + "ppsspp" + std::to_string(gameDef.ppssppSettings) + ".ini", target + "PSP/SYSTEM/ppsspp.ini");
#endif
    return 0;
}

std::string ModePspfix::getStockPath() {
    std::string path = targetDir + "/games/data/family/PSP0000/";
    return path;
}


int ModePspfix::stage1() {
    std::cout << "Installing PSP injector to " << targetDir << std::endl;

    if (!Fs::exists(targetDir)) {
        std::cout << targetDir << " does not exist, exiting." << std::endl;
        return 1;
    }

    std::string mcGames = targetDir + "/mcgames/";
    std::string pspRomFolder = mcGames + "PSP0000";
    Fs::makeDirectory(mcGames);
    Fs::makeDirectory(pspRomFolder);
    if (!Fs::exists(mcGames)) {
        std::cout << "Cannot create " << targetDir << ", exiting." << std::endl;
        return 1;
    }

    std::ofstream installFile;
    installFile.open(mcGames + "install.txt");
    installFile << "PSP0000" << std::endl;
    installFile.close();

    StickDownloader stickDl;
    StickExtractor stickExt;
    PSPMapper pm;
    std::map<std::string, downloadDefinition> balls = pm.getControlFixes();
    std::map<std::string, downloadDefinition>::iterator it;
    int result = 0;
    for (it = balls.begin(); it != balls.end(); it++) {
        std::string stickName = it->second.name;
        downloadDefinition def = it->second;
        std::cout << "Downloading " << def.name << " control files..." << std::endl;
        std::string tarGz = stickDl.download(def);
        if (!Fs::exists(tarGz)) {
            std::cout << "Could not download " << stickName << ", exiting." << std::endl;
        }
        result = stickExt.exractToFolder(def, tarGz, pspRomFolder);
        if (result == 1) {
            std::cout << "There was a problem extracting " << stickName << ", exiting." << std::endl;
        }
    }
    return result;
}

int ModePspfix::stage2() {
    if (!stockFix()) {
        return 1;
    }
    if (!otherFix()) {
        return 1;
    }
    std::cout << "** Done. Have fun!" << std::endl;
    return 0;
}

bool ModePspfix::stockFix() {
    UserFolders uf;
    std::string tempFolder = uf.getTemporaryFolder();
    std::cout << "Attempting to fix controls and performance for game:" << std::endl;
#ifndef NO_SHAREWARE_LIMIT
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "**** Did you know that ultimate-edition users also get optimized PSP performance tweaks and extra two-player game support? ****" << std::endl;
    for (int i=10;i>=1;i--) {
        std::cout << std::to_string(i) + "...";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    std::cout << std::endl;
#endif
    if (!checkStockPath()) {
        return false;
    }
    std::string path = getStockPath();
    patchControlFolder(path, path, pspConfigGameDef{0, 2, 1});

    auto pspMapper = new PSPMapper;
    auto pspGames = pspMapper->getStockGames();
    std::map<std::string, pspConfigGameDef>::iterator it;
    std::map<std::string, downloadDefinition> controlFixes = pspMapper->getControlFixes();
    std::map<std::string, downloadDefinition>::iterator pos;
    for (it = pspGames.begin(); it != pspGames.end(); it++) {
        std::string romPath = targetDir + "/games/data/family/" + it->first;
        std::string baseRom = Fs::basename(romPath);
        if (Fs::exists(romPath)) {
#ifndef NO_SHAREWARE_LIMIT
            message = "Optimizing controls for: ";
#endif
            pos = controlFixes.find(baseRom);
            if (pos == controlFixes.end()) {
                std::cout << "Patching stock rom: " << baseRom << std::endl;
                patchControlFolder(path, romPath + "/", it->second);
            } else {
                downloadDefinition def = pos->second;
                std::cout << "Patching two-player game: " << def.name << std::endl;
                if (it->second.players == 2) {
                    Fs::makeDirectory(romPath + "/2p");
                    Fs::makeDirectory(romPath + "/2p/PSP");
                    Fs::makeDirectory(romPath + "/2p/PSP/SYSTEM");
                    Fs::makeDirectory(romPath + "/2p/PSP/PPSSPP_STATE");
                    if (!def.saveState2p.empty()) {
                        replaceRomFile(romPath, def.saveState2p, Fs::stem(def.saveState0) + ".ppst",
                                       "/2p/PSP/PPSSPP_STATE/");
                        replaceRomFile(romPath, def.saveState1, "2p.ppst", "/PSP/PPSSPP_STATE/");
                    }
                    replaceRomFile(romPath, def.saveState0, Fs::stem(def.saveState0) + ".ppst", "/PSP/PPSSPP_STATE/");

                    // Multiplayer P1
                    replaceRomFile(romPath, "ppsspp10.ini", "ppsspp.ini", "/PSP/SYSTEM/");
                    replaceRomFile(romPath, "controls8.ini", "controls.ini", "/PSP/SYSTEM/");
                    // Multiplayer P2
                    replaceRomFile(romPath, "ppsspp11.ini", "ppsspp.ini", "/2p/PSP/SYSTEM/");
                    replaceRomFile(romPath, "controls9.ini", "controls.ini", "/2p/PSP/SYSTEM/");
                    // Pandory the explory was here
                    replaceRomFile(romPath, "notice.ini", "notice.txt", "/");
                    // Keyrecord
                    if (!def.keyRecord.empty()) {
                        replaceRomFile(romPath, def.keyRecord, "KeyRecord.ini", "/");
                    }
                }
            }
        }
    }
    return true;
}

void ModePspfix::replaceRomFile(const std::string &romPath, const std::string &srcSave, const std::string &targetSave,
                                const std::string &targetFolder) {
    std::cout << "  - Copying save state: " << srcSave << std::endl;
    std::string copySrcPath = getStockPath() + srcSave;
    std::string copyDestPath;
    copyDestPath += romPath;
    copyDestPath += targetFolder;
    copyDestPath += targetSave;
    if (!Fs::exists(copySrcPath) || !Fs::exists(copyDestPath)) {
        std::cout << "Error: " << std::filesystem::path(copySrcPath).lexically_normal() << " is missing or " << std::filesystem::path(copyDestPath).lexically_normal() << " could not be written " << std::endl;
        return;
    }
    Fs::copy(copySrcPath, copyDestPath);
}

bool ModePspfix::otherFix() {
    std::string path = getStockPath();
    std::cout << "Attempting to fix non-stock PSP game controls..." << std::endl;
    if (!checkStockPath()) {
        return false;
    }

    std::string romsPath = targetDir + "/games/data/family/";
    patchControlFolder(path, path, pspConfigGameDef{0, 2, 1});

    for (const auto &entry : std::filesystem::directory_iterator(romsPath)) {
        std::string romFolder = Fs::basename(entry.path().string());
        if (romFolder.substr(0, 3) == "PSP" && romFolder != "PSP0000") {
            std::cout << romFolder << std::endl;
            patchControlFolder(path, entry.path().string() + "/", pspConfigGameDef{0, 1, 1});
        }
    }
    return true;
}


