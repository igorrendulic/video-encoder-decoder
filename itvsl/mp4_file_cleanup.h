#ifndef _ITVSLLIB_MP4_CLEANUP_H_
#define _ITVSLLIB_MP4_CLEANUP_H_

namespace itvsl
{

    namespace archive
    {

        class Mp4FileCleanup
        {
        public:
            Mp4FileCleanup()
            {
            }
            ~Mp4FileCleanup()
            {
            }

            void processCleanup(std::string folder, std::string everyX);
        };
    }
}

#endif
