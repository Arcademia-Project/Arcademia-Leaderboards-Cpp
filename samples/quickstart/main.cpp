#include <arcademia_leaderboards.h>
#include <cstdio>
#include <string>
#include <iostream>

static std::string Prompt(const char* label, const std::string& current)
{
    std::cout << label << " [" << current << "]: ";
    std::string input;
    std::getline(std::cin, input);
    return input.empty() ? current : input;
}

int main()
{
    std::cout << "Arcademia Leaderboards Quick Start" << std::endl;
    std::cout << "Mode: " << (arcademia_leaderboards_mode() == ARCADEMIA_MODE_LAUNCHER ? "Launcher" : "Sandbox") << std::endl;

    std::string api_base = Prompt("API base", "https://manager.arcademia.ac");
    std::string api_key = Prompt("API key", "");
    std::string board_slug = Prompt("Board slug", "highscore");

    arcademia_leaderboards_configure(api_base.c_str(), api_key.c_str());

    std::string last_score_id;
    for (;;)
    {
        std::cout << std::endl << "1) Ping  2) Submit random score  3) Fetch test scores  4) Claim last score  5) Quit" << std::endl << "> ";
        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "1")
        {
            const char* result = arcademia_leaderboards_ping();
            std::cout << result << std::endl;
            arcademia_leaderboards_free(result);
        }
        else if (choice == "2")
        {
            long long value = 100 + (rand() % 99900);
            const char* result = arcademia_leaderboards_submit_score(board_slug.c_str(), value, "REX", nullptr, nullptr);
            std::cout << result << std::endl;
            arcademia_leaderboards_free(result);
        }
        else if (choice == "3")
        {
            const char* result = arcademia_leaderboards_get_test_scores(board_slug.c_str(), 10, 0);
            std::cout << result << std::endl;
            arcademia_leaderboards_free(result);
        }
        else if (choice == "4")
        {
            if (last_score_id.empty())
            {
                std::cout << "Submit a score first." << std::endl;
                continue;
            }
            const char* result = arcademia_leaderboards_request_claim(last_score_id.c_str());
            std::cout << result << std::endl;
            arcademia_leaderboards_free(result);
        }
        else if (choice == "5")
        {
            break;
        }
    }

    arcademia_leaderboards_shutdown();
    return 0;
}
