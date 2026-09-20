#ifndef RSVP_IMAGE_DATA_VICAR_DATA_H
#define RSVP_IMAGE_DATA_VICAR_DATA_H

#include "image_data.h"

#include <array>
#include <cstddef>
#include <string_view>
#include <unordered_map>


namespace rsvp
{

    /**
     * @brief A class to read data from Vicar/PDS files.
     *
     * The Vicar file format is laid out like so:
     *
     *                  <--------+RECSIZE+-------->
     *                 +---------------------------+
     *         ^       |                           |
     *         |       |                           |
     *         +       |                           |
     * LBLSIZE/RECSIZE |         Labels            |
     *         +       |                           |
     *         |       |                           |
     *         v       |                           |
     *                 +---------------------------+
     *         ^       |                           |
     *         |       |                           |
     *         +       |                           |
     *    NLB+(N2*N3)  |       Image area          |
     *         +       |                           |
     *         |       |                           |
     *         v       |                           |
     *                 +---------------------------+
     *         ^       |                           |
     *         |       |                           |
     *         +       |                           |
     * LBLSIZE/RECSIZE |       EOL Labels          |
     *         +       |                           |
     *         |       |                           |
     *         v       |                           |
     *                 +---------------------------+
     *
     * The image area is laid out as follows:
     *
     *        <------------------+RECSIZE+------------------>
     *        +---------------------------------------------+
     *    ^   |                                             |
     *    |   |                                             |
     *    +   |                                             |
     *   NLB  |                Binary Header                |
     *    +   |                                             |
     *    |   |                                             |
     *    +   |                                             |
     *        +-----------+---------------------------------+
     *    ^   |           |                                 |
     *    |   |           |                                 |
     *    |   |           |                                 |
     *    +   |  Binary   |         Image Pixels            |
     *  N2*N3 |  Prefix   |                                 |
     *    +   |           |                                 |
     *    |   |           |                                 |
     *    v   |           |                                 |
     *        +-----------+---------------------------------+
     *         <--+NBB+--> <----------+N1*pixel size+------>
     *
     */
    class VicarData : public ImageData
    {

    public:
        /**
         * @brief The endianness of the binary integer data.
         */
        enum IntFormat
        {
            LOW, // Default
            HIGH
        };

        /**
         * @brief The endianness of the binary floating point data.
         */
        enum RealFormat
        {
            IEEE,
            RIEEE,
            VAX
        };

        /**
         * @brief The numerical format of the binary data.
         */
        enum DataFormat
        {
            BYTE = 0,
            HALF = 1,
            WORD = 1,
            FULL = 2,
            LONG = 2,
            REAL = 3,
            DOUB = 4,
            COMP = 5,
            COMPLEX = 5
        };


        /**
         * @brief Default constructor for constructing an empty vicarfile
         */
        VicarData();

        /**
         * @brief Explicit constructor for construcint a vicarfile
         * @param samples The number of samples in the image (x dimension)
         * @param lines The number of lines in the image (y dimension)
         * @param bands The number of bands in the image (z dimension)
         * @param format The data format, either BYTE, HALF, FULL, REAL, or DOUB (COMP not supported)
         */
        VicarData(int samples, int lines, int bands, DataFormat format);

        /**
         * @brief The organization of the binary data.
         *
         * The Image Pixels can be laid out in three different formats:
         *      Band Sequential
         *      Band Interleaved by Line
         *      Band Interleaved by Pixel
         *
         * The "ORG" label specifies which of these formats has been used
         * for this file.
         *
         * ORG='BSQ' -> Band Sequential
         *  N1 = Samples (NS)
         *  N2 = Lines   (NL)
         *  N3 = Bands   (NB)
         *
         *          +----^-------------------------+
         *          |    |                         |
         *          |    +                         |
         *          |  Lines                       |
         *  Band 1  |    +                         |
         *          |    |                         |
         *          <----------+ Samples +--------->
         *          |    |                         |
         *          +----v-------------------------+
         *                      ...
         *          +----^-------------------------+
         *          |    |                         |
         *          |    +                         |
         *          |  Lines                       |
         *  Band n  |    +                         |
         *          |    |                         |
         *          <----------+ Samples +--------->
         *          |    |                         |
         *          +----v-------------------------+
         *
         * ORG='BIL' -> Band Interleaved by Line
         *  N1 = Samples (NS)
         *  N2 = Bands   (NB)
         *  N3 = Lines   (NL)
         *
         *          +----^-------------------------+
         *          |    |                         |
         *          |    +                         |
         *          |  Bands                       |
         *  Line 1  |    +                         |
         *          |    |                         |
         *          <----------+ Samples +--------->
         *          |    |                         |
         *          +----v-------------------------+
         *                      ...
         *          +----^-------------------------+
         *          |    |                         |
         *          |    +                         |
         *          |  Bands                       |
         *  Line n  |    +                         |
         *          |    |                         |
         *          <----------+ Samples +--------->
         *          |    |                         |
         *          +----v-------------------------+
         *
         * ORG='BIP' -> Band Interleaved by Pixel
         *  N1 = Bands   (NB)
         *  N2 = Samples (NS)
         *  N3 = Lines   (NL)
         *
         *          +----^-------------------------+
         *          |    |                         |
         *          |    +                         |
         *          | Samples                      |
         *  Line 1  |    +                         |
         *          |    |                         |
         *          <----------+ Bands +----------->
         *          |    |                         |
         *          +----v-------------------------+
         *                      ...
         *          +----^-------------------------+
         *          |    |                         |
         *          |    +                         |
         *          | Samples                      |
         *  Line n  |    +                         |
         *          |    |                         |
         *          <----------+ Bands +----------->
         *          |    |                         |
         *          +----v-------------------------+
         *
         */
        enum DataOrg
        {
            BSQ,
            BIL,
            BIP
        };

    private:
        /**
         * @brief Whether all four corners of a bilinear sample are inside the
         * image.
         *
         * The far corners are never nearer the origin than the near ones, so
         * four tests cover all four corners.
         */
        bool corners_in_image(const BilinearCorners &corners) const
        {
            return corners.x0 >= 0 && corners.y0 >= 0 && corners.x1 < NS &&
                corners.y1 < NL;
        }

        /**
         * @brief Bilinearly interpolate one band from corners already checked
         * to be inside the image, and a band already checked to exist.
         */
        double interpolate_corners(const BilinearCorners &corners,
                                   const int band) const
        {
            const double *plane = pixel_data.get() + pixel_index(0, 0, band);
            const double *upper = plane + static_cast<size_t>(corners.y0) * NS;
            const double *lower = plane + static_cast<size_t>(corners.y1) * NS;

            return upper[corners.x0] * corners.weight_ul +
                upper[corners.x1] * corners.weight_ur +
                lower[corners.x0] * corners.weight_ll +
                lower[corners.x1] * corners.weight_lr;
        }

        // Image comments
        std::string comments;

        std::unordered_map<std::string, std::string> unassociated_labels;

        std::unordered_map<std::string,
                           std::unordered_map<std::string, std::string>>
            label_values;

        int NL = 0; // Number of lines
        int NS = 0; // Number of samples
        int NB = 0; // Number of bands

        /**
         *                 N1 -->
         *          +------------------+
         *      N2  |                  |
         *   N3 |   |    Data Block    |
         *   |  v   |                  |
         *   |      +------------------+
         *   |      +------------------+
         *   |  N2  |                  |
         *   v  |   |    Data Block    |
         *      v   |                  |
         *          +------------------+
         */
        int N1 = 0; // Size of first (fastest-varying) dimension.
        int N2 = 0; // Size of second dimension.
        int N3 = 0; // Size of third (slowest-varying) dimension.

        int lblsize = 0; // Number of bytes of vicar header between the PDS
                         // header and the image

        int NBB = 0; // Number of bytes of binary prefix before each record
        int NLB = 0; // Number of lines of binary header at the top of the file

        std::unique_ptr<double[]>
            pixel_data; // Pixel data that has been re-shuffled and
                        // casted to double precision with BSQ
                        // organization. This is usually the most
                        // efficient format as bands are usually
                        // accessed separately therefore keeping large
                        // portions of this buffer in cache memory.

        int recsize = 0; // Size of a single record

        bool int_opposite_endian = false; // True if the integer data is
                                          // stored in a non-native endianness
        bool real_opposite_endian = false; // True if the floating-point data
                                           // is stored in a non-native
                                           // endianness

        DataFormat format = BYTE;
        DataOrg org = BSQ;

        IntFormat intfmt = LOW;
        RealFormat realfmt = IEEE;

        /**
         * @brief The offset of a pixel within `pixel_data`, which is always
         * organized as BSQ (no interlacing).
         */
        size_t pixel_index(int sample, int line, int band) const
        {
            return static_cast<size_t>(band) * NL * NS +
                static_cast<size_t>(line) * NS + static_cast<size_t>(sample);
        }

    protected:
        /**
         * @brief Assign the header labels / comments for this image.
         *
         * @param comments A string of the text headers at the beginning of
         * the file.
         */
        void set_labels(std::string comments);

        int get_n1() const
        {
            return N1;
        }

        int get_n2() const
        {
            return N2;
        }

        int get_n3() const
        {
            return N3;
        }

    public:
        /**
         * @brief A static factory method to read a Vicar or PDS file
         *
         * @param path The absolute filepath of the image to read
         *
         * @return A pointer to a new VicarData. Throws an std::runtime_error
         * if an error occurred.
         */
        static std::shared_ptr<VicarData>
        read_vicarfile(const std::string &path);

        /*
         * @brief Create and write the current VicarData object to the file at path
         *
         * @param path The filepath of the image to read
         */
        void write_vicarfile(const std::string &path) const;

        /*
         * @brief Get a pixel of the image as a double value
         *
         * @param value Output variable, the value of the pixel
         * @param sample The x coord of the pixel
         * @param line The y coord of the pixel
         * @param band The z coord of the pixel
         *
         * @return A boolean indicating whether the pixel was successfully gotten
         */
        inline bool get_pixel_double(double &value,
                                     int sample,
                                     int line,
                                     int band) const override
        {
            if (line < 0 || sample < 0 || band < 0)
            {
                return false;
            }

            if (line >= NL || sample >= NS || band >= NB)
            {
                return false;
            }

            value = pixel_data[pixel_index(sample, line, band)];
            return true;
        }

        /**
         * @brief Bilinearly interpolate straight out of the pixel buffer.
         *
         * The default implementation fetches each of the four corners through
         * a virtual call that checks its bounds on its own. This is the image
         * at the bottom of every terrain stack, so check the four corners at
         * once and read them directly. The arithmetic is the same as the
         * default's, in the same order, so the result is identical.
         */
        bool get_interpolated_pixel_double(double &value,
                                           double x,
                                           double y,
                                           int band) const override
        {
            if (!get_interpolating())
            {
                return get_pixel_double(
                    value, round_to_int(x), round_to_int(y), band);
            }

            const BilinearCorners corners = bilinear_corners(x, y);

            if (!corners_in_image(corners) || band < 0 || band >= NB)
            {
                return false;
            }

            value = interpolate_corners(corners, band);
            return true;
        }

        /**
         * @brief Interpolate several bands from the same four corners.
         *
         * @see ImageData::get_interpolated_bands_double
         */
        bool get_interpolated_bands_double(double *values,
                                           const int *bands,
                                           const int count,
                                           const double x,
                                           const double y) const override
        {
            if (!get_interpolating())
            {
                const int sample = round_to_int(x);
                const int line = round_to_int(y);

                for (int i = 0; i < count; i++)
                {
                    if (!get_pixel_double(values[i], sample, line, bands[i]))
                    {
                        return false;
                    }
                }

                return true;
            }

            const BilinearCorners corners = bilinear_corners(x, y);

            if (!corners_in_image(corners))
            {
                return false;
            }

            for (int i = 0; i < count; i++)
            {
                if (bands[i] < 0 || bands[i] >= NB)
                {
                    return false;
                }

                values[i] = interpolate_corners(corners, bands[i]);
            }

            return true;
        }

        /*
         * @brief Set a pixel of the image from a double value
         *
         * @param value Input variable, the value of the pixel
         * @param sample The x coord of the pixel
         * @param line The y coord of the pixel
         * @param band The z coord of the pixel
         *
         * @return A boolean indicating whether the pixel was successfully set
         */
        bool set_pixel_double(const double value,
                              const int sample,
                              const int line,
                              const int bind);

        /**
         * @brief Get the image labels for this VicarFile.
         *
         * @return The image labels.
         */
        std::string get_labels() const
        {
            return comments;
        }

        /**
         * @brief Get a label property by name, if present.
         *
         * @param property_group Property group of interest.
         * @param property_name Name of the property within the group.
         * @param value Reference to a string to be filled with the value.
         *
         * @return True if the property was found, false otherwise.
         */
        bool get_label_property(const std::string &property_group,
                                const std::string &property_name,
                                std::string &value) const;

        /**
         * @brief Set an unassociated label property name, creates if not present
         *
         * @param property_name Name of the property
         * @param value Reference to a string to be filled with the value.
         */
        void set_label_unassociated_property(
            const std::string &property_name, const std::string &value);

        /**
         * @brief Get an indexed label property by name, if present.
         *
         * @param property_group Property group of interest.
         * @param property_name Name of the property within the group.
         * @param index The index the property array of interest.
         * @param value Reference to a string to be filled with the value.
         *
         * @return True if the property was found, false otherwise.
         */
        bool get_label_indexed_property(const std::string &property_group,
                                        const std::string &property_name,
                                        unsigned int index,
                                        std::string &value) const;

        /**
         * @brief Check if the binary integer data is stored in the
         * opposite endianness than the host.
         *
         * @return true if the binary integer data is stored in a different
         * endianness convention than the host architecture.
         */
        bool is_int_opposite_endian() const
        {
            return int_opposite_endian;
        }

        /**
         * @brief Check if the binary floating point data is stored in the
         * opposite endianness than the host.
         *
         * @return true if the binary floating point data is stored in a
         * different endianness convention than the host architecture.
         */
        bool is_real_opposite_endian() const
        {
            return real_opposite_endian;
        }

        /**
         * @brief Get the size of an individual data record.
         *
         * @return RECSIZE
         */
        int get_record_size() const
        {
            return recsize;
        }

        /**
         * @brief Get the number of lines of binary labels.
         *
         * @return NLB
         */
        int get_binary_label_line_count() const
        {
            return NLB;
        }

        /**
         * @brief Get the number of bytes of binary prefix per record.
         *
         * @return NBB;
         */
        int get_binary_prefix_byte_count() const
        {
            return NBB;
        }

        /**
         * @brief Get the pixel organization (BSQ, BIL, BIP).
         *
         * @return org;
         */
        DataOrg get_org() const
        {
            return org;
        }

        /**
         * @brief Get the number of bytes of data per pixel.
         *
         * Floats have 4 bytes, shorts have 2 bytes, etc.
         *
         * @return The number of bytes of binary data per pixel
         */
        inline int get_pixel_byte_count() const
        {
            switch (format)
            {
            case BYTE:
                return 1;
            case HALF:
                return 2;
            case FULL:
            case REAL:
                return 4;
            case DOUB:
            case COMP: // This is a weird case - it is two floats...
                return 8;
            }

            return 0;
        }

        /**
         * @brief Get the number of lines of data.
         *
         * @return NL
         */
        int get_height() const override
        {
            return NL;
        }

        /**
         * @brief Get the number of samples of data.
         *
         * @return NS
         */
        int get_width() const override
        {
            return NS;
        }

        /**
         * @brief Get the number of bands of data.
         *
         * @return NB
         */
        int get_bands() const override
        {
            return NB;
        }

        /**
         * @brief The pixel grid, in pixel indices.
         *
         * A bare VICAR image is looked up by sample and line, so that is where
         * its pixels are. Where its labels say it sits in the world is
         * `get_map_bounds`.
         */
        TerrainBounds get_bounds() const override
        {
            return pixel_grid_bounds();
        }

        /**
         * @brief Where the SURFACE_PROJECTION_PARMS labels place this image
         * in the world.
         *
         * Not `get_bounds`: lookups on a bare VICAR image take pixel indices,
         * not world coordinates, so a composite must not skip it by these.
         *
         * @return Bounds over the pixel centers, in meters, or invalid bounds
         * if the labels do not place the image.
         */
        TerrainBounds get_map_bounds() const;

        bool get_camera_cahv_frame(std::string &frame) const;

        bool get_camera_c(double camera_c[3]) const;
        bool get_camera_a(double camera_a[3]) const;
        bool get_camera_h(double camera_h[3]) const;
        bool get_camera_v(double camera_v[3]) const;
        bool get_camera_o(double camera_o[3]) const;
        bool get_camera_r(double camera_r[3]) const;
        bool get_camera_e(double camera_e[3]) const;
    };
}

#endif
