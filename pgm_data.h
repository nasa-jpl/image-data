#ifndef RSVP_IMAGE_DATA_PGM_DATA_H
#define RSVP_IMAGE_DATA_PGM_DATA_H

#include "image_data.h"

#include <cstdint>


namespace rsvp
{

    class PGMData : public ImageData
    {
        /*  Per netpbm.sourceforge.net/doc/pgm.html, the PGM (Portable GrayMap)
         * file format is as follows:
         *
         *  1. A "magic number" for identifying the file type. A pgm image's
         * magic number is the two characters "P5".
         *  2. Whitespace (blanks, TABs, CRs, LFs).
         *  3. A width, formatted as ASCII characters in decimal.
         *  4. Whitespace.
         *  5. A height, again in ASCII decimal.
         *  6. Whitespace.
         *  7. The maximum gray value (Maxval), again in ASCII decimal. Must be
         * less than 65536, and more than zero.
         *  8. A single whitespace character (usually a newline).
         *  9. A raster of Height rows, in order from top to bottom. Each row
         *     consists of Width gray values, in order from left to right. Each
         *     gray value is a number from 0 through Maxval, with 0 being black
         *     and Maxval being white. Each gray value is represented in pure
         *     binary by either 1 or 2 bytes. If the Maxval is less than 256,
         * it is 1 byte. Otherwise, it is 2 bytes. The most significant byte is
         *     first.
         *
         *  In addition:
         *
         *  Before the whitespace character that delimits the raster (#8
         *  above), any characters from a "#" through the next carriage return
         *  or newline character, is a comment and is ignored. Note that this
         *  is rather unconventional, because a comment can actually be in the
         *  middle of what you might consider a token. Note also that this
         *  means if you have a comment right before the raster, the newline at
         *  the end of the comment is not sufficient to delimit the raster.
         *
         *  A row of an image is horizontal. A column is vertical. The pixels
         *  in the image are square and contiguous.
         *
         *  Each gray value is a number proportional to the intensity of the
         *  pixel, adjusted by the ITU-R Recommendation BT.709 gamma transfer
         *  function. (That transfer function specifies a gamma number of 2.2
         *  and has a linear section for small intensities). A value of zero is
         *  therefore black. A value of Maxval represents CIE D65 white and the
         *  most intense value in the image and any other image to which the
         *  image might be compared.  BT.709's range of channel values (16-240)
         *  is irrelevant to PGM.
         *
         *  Note that a common variation from the PGM format is to have the
         *  gray value be "linear," i.e. as specified above except without the
         *  gamma adjustment. pnmgamma takes such a PGM variant as input and
         *  produces a true PGM as output.  Another popular variation from PGM
         *  is to substitute the newer sRGB transfer function for the BT.709
         *  one. You can use pnmgamma to convert between this variation and
         *  true PGM.
         *
         *  Strings starting with "#" may be comments, the same as with PBM.
         */
    private:
        // Pixel count for image dimensions
        int width;
        int height;

        // PGM requires file to note their maximum value
        // This value must be above 0 and below 65536
        int maximum_value;

        // PGM files have either 8-bit or 16-bit pixel width
        int pixel_byte_count;

        // Offset in file for pixel data
        int data_offset;

        // Raw data from the file
        // Pixels here will be casted and byte-swapped
        // into the pixel_array
        uint8_t *pixel_array_8;
        uint16_t *pixel_array_16;

        // PGM is always stored in big-endian
        // Flip if host is little-endian
        bool opposite_endian;

        PGMData();

    public:
        static std::shared_ptr<PGMData> read_pgm(const std::string &filename);

        ~PGMData();

        int get_width() const override
        {
            return width;
        }

        int get_height() const override
        {
            return height;
        }

        int get_max_value() const
        {
            return maximum_value;
        }

        int get_pixel_byte_count() const
        {
            return pixel_byte_count;
        }

        int get_bands() const override
        {
            return 1;
        }

        // Return an exact pixel value as a double
        bool get_pixel_int(int &value,
                           int sample,
                           int line,
                           int band) const override;

        // PGM files can only store integer data, so up-cast the integer value
        bool get_pixel_double(double &value,
                              const int sample,
                              const int line,
                              const int band) const override
        {
            int intermediate = 0;
            bool status = get_pixel_int(intermediate, sample, line, band);

            value = static_cast<double>(intermediate);
            return status;
        }
    };

}

#endif
