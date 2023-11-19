

//Simple purpose console program which I can use to backup saves for various games.  Will store a file with a series of save data locations based on searching them with this program.
#include "nfd/nfd.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <Windows.h>

#define DEFAULT_BACKUP_SAVE_LIMIT 5

//Ignore some deprecation warnings

bool compareTimestamps(const std::string& path1, const std::string& path2);
std::string GetCurrentDateTimeAsString();


static bool exit_program = false;
static std::unordered_map<std::string, std::string> save_paths;
static int backup_save_limit = DEFAULT_BACKUP_SAVE_LIMIT;

//A signal handled function that should ALWAYS run at the end of the program REGARDLESS of how we are closed UNLESS by Task Manager
// This makes sense b/c a user SHOULD expect program state to break or not do things if they intentionally force closed it.
// We are mostly guarding against accidentally closing the app (Ctrl + C in terminal/cmd, "X" button on console", or normal closing of app)
void ProgramExitLastSteps()
{
    std::filesystem::path textFilePath("./savefolders.ini");
    std::ofstream outputFileStream;

    //creates file if doesn't exist
    outputFileStream.open(textFilePath, std::ios::out);

    if (outputFileStream.is_open())
    {
        //Before exiting, rewrite the save paths folder to include all of the new paths into the file list.
        for (auto key_value_pair : save_paths)
        {
            outputFileStream << key_value_pair.first << " = " << key_value_pair.second << "\n";
        }
        outputFileStream.close();
    }
    else
    {
        std::cerr << "Error creating save folders .ini file." << std::endl;
    }
}

//Just for registering console close commands.
BOOL onConsoleEvent(DWORD event) {

    switch (event) {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
        ProgramExitLastSteps();
        break;
    }

    return TRUE;
}

void signalHandler(int signum) {
    // Make sure create file with all paths we've saved.
    ProgramExitLastSteps();

    // Exit the program
    std::exit(signum);
}


int main()
{
    //==========================================================
    //  Register signal exits so we can ALWAYS run final code.
    //  (like saving paths to our ini file)
    //==========================================================
    
    std::atexit(ProgramExitLastSteps);              // Before program exits normally, always run this
    std::signal(SIGTERM, signalHandler);            // Termination request, including console window closing
    std::signal(SIGINT, signalHandler);             // Interrupt signal (Ctrl+C)
    SetConsoleCtrlHandler(onConsoleEvent, TRUE);    // Handle Console Window closing

    
    //==========================================================
    //  Load savefolders.ini config file
    //==========================================================

    std::filesystem::path textFilePath("./savefolders.ini");
    std::ifstream inputFileStream;
    inputFileStream.open(textFilePath, std::ios::in);

    if (inputFileStream.is_open())
    {
        //Read every file path into our list
        std::string line;
        int part = 0;

        while (std::getline(inputFileStream, line))
        {

            //every even iteration/part is a key, every odd iteration is the value
            //we are also cutting the spaces around the '=' sign.
            std::istringstream iss(line);
            std::string key, value;
            if (std::getline(iss >> std::ws, key, '=') && std::getline(iss >> std::ws, value)) {
                // Process key and value

                //Remove trailing whitespace
                while (!key.empty() && std::isspace(key.back())) {
                    key.pop_back();
                }

                while (!value.empty() && std::isspace(value.back())) {
                    value.pop_back();
                }

                save_paths[key] = value;
            }

            part++;
        }

        inputFileStream.close();

        std::cout << "Successfully loaded " << part << " save backup path(s) from configuration." << std::endl;
        std::cout << std::endl;
    }
    else
    {
        std::cout << "Didn't find savefolders.ini file.  No save data backup locations were loaded." << std::endl;
    }


    //==========================================================
    //  Run main program loop
    //==========================================================

    //NEED TO UPDATE THIS WHEN WE ADD MORE OPTIONS.
    int max_options = 4;

    while (!exit_program)
    {
        std::cout << "Save Backup Manager:" << std::endl <<
                     "--------------------" << std::endl <<
                     "1. Choose a new folder to add to the managed save backups list." << std::endl <<
                     "2. List all backup games and their paths." << std::endl <<
                     "3. Backup all new saves." << std::endl <<
                     "4. Exit program." << std::endl <<
                     std::endl;

        std::string userInput;

        std::cin >> userInput;

        int convertedNumber = 0;
        try
        {
            convertedNumber = std::stoi(userInput);
        }
        catch (const std::exception& ex)
        {
                system("cls");
                std::cerr << "Invalid input, '" << userInput << "'." << std::endl;
                std::cerr << "Enter a number corresponding to one of the options." << std::endl;
                std::cout << "\n\n";
                continue;
        }

        if (convertedNumber <= 0 || convertedNumber > max_options)
        {
            system("cls");
            std::cerr << "Invalid input, '" << userInput << "'." << std::endl;
            std::cerr << "Enter a number corresponding to one of the options." << std::endl;
            std::cout << "\n\n";
            continue;
        }

        //If no issues, then handle each possible argument

        switch (convertedNumber)
        {
            //==========================================================
            //  Choose a new folder to store backups of.
            //==========================================================
            case 1:
            //First open file dialog which will ask the user to select a folder where saves are located that should be backed up.
            {
                nfdchar_t* save_path = NULL;
                std::string file_result_text;
                nfdresult_t result = NFD_PickFolder(NULL, &save_path);

                if (result == NFD_OKAY)
                {
                    //Actually add this to a list and save it to a file we can load on start up next time.
                    std::string selected_path = std::string(save_path);

                    std::vector<std::string> all_save_folders;
                    std::vector<std::string> all_save_game_names;
                    for (const auto& pair : save_paths)
                    {
                        all_save_game_names.push_back(pair.first);  //keys
                        all_save_folders.push_back(pair.second);    //values
                    }

                    auto iter = std::find(all_save_folders.begin(), all_save_folders.end(), selected_path);

                    //If we didn't find the file, okay to move on
                    if (iter == all_save_folders.end())
                    {
                        //Ask what name should be used for this.
                        bool validNameInput = false;
                        std::string userInputGameName;
                        while (!validNameInput)
                        {
                            std::cout << "Enter the name you want to associate this save data with (a folder with this name will be created when backing up saves." << std::endl;
                            //Need to include spaces and other stuff except newline
                            std::getline(std::cin >> std::ws, userInputGameName);

                            //Remove trailing whitespace
                            while (!userInputGameName.empty() && std::isspace(userInputGameName.back())) {
                                userInputGameName.pop_back();
                            }

                            //Must first check if this name already exists in the list, if so, need a new name or else stuff gets overwritten.
                            auto inputIter = std::find(all_save_game_names.begin(), all_save_game_names.end(), userInputGameName);
                            if (inputIter != all_save_game_names.end())
                            {
                                std::cerr << "A backup save folder with game name, \"" << userInputGameName << "\", already exists.  Please enter a new game name." << std::endl;
                                std::cout << std::endl;
                            }
                            else
                            {
                                validNameInput = true;
                            }
                        }

                        save_paths[userInputGameName] = selected_path;
                        file_result_text = "Added \"" + selected_path + "\" to save backup path list with the name: \"" + userInputGameName + "\"";
                    }
                    else
                    {
                        //value already exists in the list, so ignore it
                        file_result_text = "Save folder, \"" + selected_path + "\" already exists in the stored save file paths backed up.";
                    }
                }
                else if (result == NFD_CANCEL)
                {
                    file_result_text = "User cancelled selection, no save backup file path was added.";
                }

                if (save_path != NULL)
                {
                    free(save_path);
                }

                system("cls");
                if (!file_result_text.empty())
                {
                    std::cout << file_result_text << std::endl;
                    std::cout << "\n\n";
                }
                //Get from the user the game's name b/c where .exes are located differ between games and also not all .exes are named coherently.
                break;
            }

            
            //==========================================================
            //  List all existing game save backups kept track of
            //==========================================================
            case 2:
            {
                std::cout << "Saves Managed" << std::endl <<
                             "---------------" << std::endl <<
                             "Game Name           |             Save Game Path" << std::endl <<
                             "-------------------------------------------------------------" << std::endl;
                for (auto save_path : save_paths)
                {
                    std::cout << save_path.first << " | " << save_path.second << std::endl;
                }
                std::cout << "\n\n";
                break;
            }

            //==========================================================
            //  Backup all new saves for managed games
            //==========================================================
            case 3:
            {
                std::vector<std::string> all_save_game_names;
                for (const auto& pair : save_paths)
                {
                    all_save_game_names.push_back(pair.first);  //keys
                }

                std::vector<std::string> game_saves_updated;
                bool backup_performed = false;

                //Loop through the save games and update save game backups
                for (auto save_game_name : all_save_game_names)
                {
                    //Get current time and append to the path for our save backup
                    std::filesystem::path backup_folder = "./Backups/" + save_game_name;
                    std::filesystem::path backup_path = backup_folder.generic_string() + "/" + "Backup" + " - " + GetCurrentDateTimeAsString();

                    if (!std::filesystem::exists(backup_folder))
                    {
                        //Create this save game backup folder if doesn't exist
                        std::filesystem::create_directories(backup_folder);
                    }

                    //Count how many backups exist already
                    int count = 0;
                    std::string backup_name = "Backup";
                    std::vector<std::string> backup_folder_paths;

                    for (const auto& entry : std::filesystem::directory_iterator(backup_folder))
                    {
                        if (std::filesystem::is_directory(entry))
                        {
                            // Check if the substring exists in the directory name
                            if (entry.path().filename().string().find(backup_name) != std::string::npos)
                            {
                                backup_folder_paths.push_back(entry.path().generic_string());
                                count++;
                            }
                        }
                    }

                    std::sort(backup_folder_paths.begin(), backup_folder_paths.end(), compareTimestamps);

                    //If count >= save limit, remove earliest ones until we have save_limit - 1 (b/c need to make new one)
                    if (count > backup_save_limit)
                    {
                        for (auto path : backup_folder_paths)
                        {
                            if (count == backup_save_limit - 1)
                            {
                                break;
                            }

                            std::filesystem::path path_to_remove(path);
                            std::filesystem::remove_all(path_to_remove);
                            count--;
                        }
                    }


                    //Check if the backup exists/needs to be made at all
                    std::filesystem::path actual_save_path = save_paths[save_game_name];

                    if (std::filesystem::exists(actual_save_path))
                    {
                        //Create the time stamped folder first
                        std::filesystem::create_directories(backup_path);

                        //Make root directory of save folder
                        std::string save_dir = std::filesystem::relative(actual_save_path, actual_save_path.parent_path()).generic_string();
                        std::filesystem::path backup_final_directory(backup_path.generic_string() + "/" + save_dir);
                        std::filesystem::create_directories(backup_final_directory);

                        //Attempt to back up the save data inside the root save folder.
                        for (const auto& entry : std::filesystem::recursive_directory_iterator(actual_save_path))
                        {
                            const std::filesystem::path& currentPath = entry.path();
                            const std::filesystem::path relativePath = std::filesystem::relative(currentPath, actual_save_path);
                            const std::filesystem::path destinationPath = backup_final_directory / relativePath;

                            try
                            {
                                if (std::filesystem::is_directory(entry.status())) {
                                    std::filesystem::create_directories(destinationPath);
                                }
                                else if (std::filesystem::is_regular_file(entry.status())) {
                                    std::filesystem::copy_file(currentPath, destinationPath, std::filesystem::copy_options::overwrite_existing);
                                }
                            }
                            catch (const std::exception& e)
                            {

                                //if we failed to do so, remove the backup folder we created and all data we tried to backup there
                                std::cerr << "Error copying file " << currentPath << ": " << e.what() << std::endl;
                                std::cout << std::endl;
                                std::cout << "Deleting backup that was attempted..." << std::endl;
                                
                                std::filesystem::remove_all(backup_path);

                                std::cout << "Deleted incomplete backup data." << std::endl;
                            }
                        }

                        game_saves_updated.push_back(save_game_name);

                        backup_performed = true;
                    }
                    else
                    {
                        std::string userAnswer = "";

                        while (userAnswer != "y" && userAnswer != "n")
                        {
                            std::cerr << "The backup save file location for \"" << save_game_name << "\" -> \"" << actual_save_path << "\" doesn't exist." << std::endl;
                            std::cout << std::endl;
                            std::cout << "Should we remove this backup path from the configuration? (y/n) -> ";

                            std::cin >> userAnswer;

                            if (userAnswer == "y")
                            {
                                //Remove this backup -- the actual config file will be updated whenever the dictionary is saved later.
                            }
                            else if (userAnswer == "n")
                            {
                                std::cout << "Despite the save data path not existing for \"" << save_game_name << "\", the save path will be kept in your configuration." << std::endl;
                            }
                            else
                            {
                                system("cls");
                                std::cerr << "Please enter a correct answer." << std::endl;
                                std::cout << std::endl;
                            }
                        }
                    }
                }
                system("cls");

                if (backup_performed)
                {
                    std::cout << "Backups made for the following games:" << std::endl <<
                                 "-------------------------------------" << std::endl;

                    for (const auto& name : game_saves_updated)
                    {
                        std::cout << name << std::endl;
                    }
                    std::cout << std::endl;
                }
                else
                {
                    //TODO: Currently, we aren't actually checking if the files are matching with MD5... so as for now this is kind of irrelevant.
                    std::cout << "No save data needed backing up." << std::endl;
                    std::cout << std::endl;
                }

                break;
            }

            //==========================================================
            //  Exit the program
            //==========================================================
            case 4:
            {
                exit_program = true;
                system("cls");
                std::cout << "Exiting..." << std::endl;

                //We use a signal to ensure file writes happen regardless of whether app is closed early or by this option.
                break;
            }
        }
    };
}



//==========================================================
//    Helpers
//==========================================================

//Gets current time as a string
std::string GetCurrentDateTimeAsString() {


    // Get the current time
    auto currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::stringstream ss;

    // Convert the time to a struct tm using localtime_s
    struct std::tm timeInfo = {};
    if (localtime_s(&timeInfo, &currentTime) == 0)
    {
        // Create a stringstream to format the date and time
        ss << std::put_time(&timeInfo, "%Y-%m-%d %Hh%Mm%Ss");
    }
    else
    {
        // Handle the case where localtime fails
        std::cerr << "Error getting local time." << std::endl;
    }
    // Convert stringstream to string
    return ss.str();
}

bool compareTimestamps(const std::string& path1, const std::string& path2) {
    // Extract timestamps from the paths
    auto extractTimestamp = [](const std::string& path) {
        std::tm timestamp = {};
        sscanf_s(path.c_str(), "Backup - %d-%d-%d %dh%dm%ds",
            &timestamp.tm_year, &timestamp.tm_mon, &timestamp.tm_mday,
            &timestamp.tm_hour, &timestamp.tm_min, &timestamp.tm_sec);
        timestamp.tm_year -= 1900; // Adjust year
        timestamp.tm_mon -= 1;    // Adjust month
        return std::mktime(&timestamp);
        };

    // Compare timestamps
    return extractTimestamp(path1) < extractTimestamp(path2);
}