#ifndef RSVP_IMAGE_DATA_MOD_DATA_H
#define RSVP_IMAGE_DATA_MOD_DATA_H

#include "image_data.h"


namespace rsvp
{

    /**
     * @brief A class to read .mod files.
     */
    class ModData
    {

    protected:
        static std::shared_ptr<ImageData>
        parse_imagedata(std::list<std::string> *tokens,
                        const std::string &filename,
                        const std::string &terrain_blending_mode);
        static std::shared_ptr<ImageData>
        parse_filedata(std::list<std::string> *tokens,
                       const std::string &filename,
                       const std::string &terrain_blending_mode);
        static std::shared_ptr<ImageData>
        parse_compdata(std::list<std::string> *tokens,
                       const std::string &filename,
                       const std::string &terrain_blending_mode);
        static std::shared_ptr<ImageData>
        parse_transdata(std::list<std::string> *tokens,
                        const std::string &filename,
                        const std::string &terrain_blending_mode);
        static std::shared_ptr<ImageData>
        parse_zoffset(std::list<std::string> *tokens,
                      const std::string &filename,
                      const std::string &terrain_blending_mode);
        static std::shared_ptr<ImageData>
        parse_deinterpolate(std::list<std::string> *tokens,
                            const std::string &filename,
                            const std::string &terrain_blending_mode);

    public:
        /**
         * @brief A static factory method to read a .mod file.
         *
         * @param filename The absolute filename to the .mod file.
         * @param terrain_blending_mode The blending mode to use for
         * compositing terrains ("alpha" or "distance")
         *
         * @return A shared pointer to a newly constructed ImageData. Throws an
         * std::runtime_error if an error occurred.
         */
        static std::shared_ptr<ImageData>
        read_modfile(const std::string &filename,
                     const std::string &terrain_blending_mode = "alpha");

        /**
         * @brief A static factory method to read a .ht or .tc file and
         * transform it appropriately, according to the values in the image
         * header.
         *
         * @param filename The absolute filename to the vicar file.
         * @param terrain_blending_mode The blending mode to use for
         * compositing terrains ("alpha" or "distance")
         *
         * @return A shared pointer to a newly constructed ImageData. Throws an
         * std::runtime_error if an error occurred.
         */
        static std::shared_ptr<ImageData> read_bare_vicarfile(
            const std::string &filename,
            const std::string &terrain_blending_mode = "alpha");
    };
}

#endif
