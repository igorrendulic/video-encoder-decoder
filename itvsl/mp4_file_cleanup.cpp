#include <chrono>
#include <dirent.h>
#include <thread>
#include <iostream>
#include <stdio.h>
#include <bits/stdc++.h>

#include "mp4_file_cleanup.h"
#include "video_log.h"

using namespace std::chrono_literals; // ns, us, ms, s, h, etc.
using namespace itvsl::archive;
using namespace itvsl::log;
using namespace std;

// parse duration parses duration (e.g. 5m, 10d, 3s, 1w, 0.2w, 5000ms), returns miliseconds
static int64_t parseDuration(std::string olderThan)
{

    auto defaultDuration = 300000ms; // 5 minutes

    std::string alpha, num;
    for (int i = 0; i < olderThan.length(); i++)
    {
        if (isdigit(olderThan[i]))
        {
            num.push_back(olderThan[i]);
        }
        else if ((olderThan[i] >= 'A' && olderThan[i] <= 'Z') || (olderThan[i] >= 'a' && olderThan[i] <= 'z'))
        {
            alpha.push_back(olderThan[i]);
        }
        else
        {
            logging("parsing of olderThan field failed, using default of 5m");
            // parsing failed
            return defaultDuration.count();
        }
    }

    try
    {
        if (alpha.empty())
        {
            return defaultDuration.count();
        }

        std::transform(alpha.begin(), alpha.end(), alpha.begin(), ::tolower);

        double dur = atof(num.c_str());

        if (alpha == "m")
        {
            return std::round(dur * 60 * 1000);
        }
        else if (alpha == "s")
        {
            return std::round(dur * 1000);
        }
        else if (alpha == "d")
        {
            return std::round(dur * 24 * 60 * 60 * 1000);
        }
        else if (alpha == "h")
        {
            return std::round(dur * 24 * 60 * 60 * 1000);
        }
        else if (alpha == "w")
        {
            return std::round(dur * 7 * 24 * 60 * 60 * 1000);
        }
        else if (alpha == "ms")
        {
            return std::round(dur);
        }
        else
        {
            throw "unrecognized duration";
        }
    }
    catch (std::exception &e)
    {
        logging("failed to parse cleanup duration, using default", e.what());
    }

    return defaultDuration.count();
}

void Mp4FileCleanup::processCleanup(std::string folder, std::string olderThan)
{

    // calculate how old mp4 files should be deleted (files created before removeBeforeThisTime)
    int64_t removeOlderThan = parseDuration(olderThan);

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // repeat every 5 seconds

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        int64_t removeBeforeThisTime = ms.count() - removeOlderThan;

        logging("running mp4 file cleanup");

        DIR *di;
        char *ptr1, *ptr2;
        int retn;

        struct dirent *dir;

        std::vector<std::string> oldFiles;

        char filepath[1024];

        di = opendir(folder.c_str()); // specify the directory name
        if (di)
        {
            while ((dir = readdir(di)) != NULL)
            {
                ptr1 = strtok(dir->d_name, ".");
                ptr2 = strtok(NULL, ".");
                if (ptr2 != NULL)
                {
                    retn = strcmp(ptr2, "mp4");
                    if (retn == 0)
                    {
                        try
                        {
                            int64_t file_age = strtoll(ptr1, NULL, 10);
                            if (file_age <= removeBeforeThisTime)
                            {
                                sprintf(filepath, "%s/%s.%s", folder.c_str(), dir->d_name, "mp4");
                                remove(filepath);
                                logging("removed file %s", filepath);
                            }
                        }
                        catch (std::exception &e)
                        {
                            logging("failed to convert to number mp4 file. skipping it...", ptr1);
                        }
                    }
                }
            }
            closedir(di);
        }
    }
}