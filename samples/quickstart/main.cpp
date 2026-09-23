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
        std::cout << std::endl << "1) Ping  2) Submit random score  3) Fetch test scores  4) Claim last score  5) Quit  6) Fetch every scope" << std::endl << "> ";
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
            std::string text = result;
            auto key = text.find("\"ScoreId\":\"");
            if (key != std::string::npos)
            {
                auto start = key + 11;
                last_score_id = text.substr(start, text.find('"', start) - start);
            }
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
        else if (choice == "6")
        {
            const int scopes[] = { ARCADEMIA_SCOPE_LOCAL, ARCADEMIA_SCOPE_INSTITUTIONAL, ARCADEMIA_SCOPE_COUNTRY, ARCADEMIA_SCOPE_GLOBAL };
            for (int scope : scopes)
            {
                const char* result = arcademia_leaderboards_get_scores(
                    board_slug.c_str(), scope, "1-5", last_score_id.empty() ? nullptr : last_score_id.c_str(), 2, 2, 1);
                std::cout << result << std::endl;
                arcademia_leaderboards_free(result);
            }
        }
    }

    arcademia_leaderboards_shutdown();
    return 0;
}
