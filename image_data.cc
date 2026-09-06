#include "image_data.h"

#include "csv_data.h"
#include "mod_data.h"
#include "pgm_data.h"
#include "vicar_data.h"

#include <stdexcept>

#include <algorithm>
#include <cmath>
#include <limits>

namespace rsvp
{

    ImageData::ImageData() :
        alpha_band(-1),
        interpolate(true)
    {
    }

    ImageData::~ImageData() = default;

    std::shared_ptr<ImageData> ImageData::read(const std::string &filename)
    {
        std::shared_ptr<ImageData> return_value;
        std::string extension =
            filename.substr(filename.find_last_of('.') + 1);
        if (extension == "mod" || extension == "MOD" ||
            extension == "mod_tc" || extension == "MOD_TC")
        {
            return_value = ModData::read_modfile(filename);
        }
        else if (extension == "img" || extension == "IMG" ||
                 extension == "vic" || extension == "VIC")
        {
            return_value = VicarData::read_vicarfile(filename);
        }
        else if (extension == "ht" || extension == "HT" || extension == "tc" ||
                 extension == "TC")
        {
            return_value = ModData::read_bare_vicarfile(filename);
        }
        else if (extension == "csv" || extension == "CSV")
        {
            return_value = CSVData::read_csv(filename);
        }
        else if (extension == "pgm" || extension == "PGM")
        {
            return_value = PGMData::read_pgm(filename);
        }
        else
        {
            throw std::runtime_error("Unrecognized file type: " + filename);
        }

        if (!return_value)
        {
            throw std::runtime_error("Reached parsing of invalid " + filename +
                                     ".");
        }

        return return_value;
    }

    bool ImageData::get_interpolated_pixel_double(double &value,
                                                  const double x,
                                                  const double y,
                                                  const int band) const
    {
        // If we've been asked not to interpolate data, snap to the nearest
        // whole pixel and return that data
        if (!get_interpolating())
        {
            int x_uninterp =
                x > 0 ? static_cast<int>(x + 0.5) : static_cast<int>(x - 0.5);
            int y_uninterp =
                y > 0 ? static_cast<int>(y + 0.5) : static_cast<int>(y - 0.5);
            return get_pixel_double(value, x_uninterp, y_uninterp, band);
        }

        int int_x = static_cast<int>(x);
        int int_y = static_cast<int>(y);

        if (int_x < 0.0)
        {
            // We sample at (int_x) and (int_x+1), so make sure that
            // those two span the input
            int_x--;
        }

        if (int_y < 0.0)
        {
            // We sample at (int_y) and (int_y+1), so make sure that
            // those two span the input
            int_y--;
        }

        double frac_x = fabs(x - int_x);
        double frac_y = fabs(y - int_y);

        // Upper left (x, y)
        double ul = 0.0;
        if (!get_pixel_double(ul, int_x, int_y, band))
        {
            return false;
        }
        double ul_weight = (1.0 - frac_x) * (1.0 - frac_y);

        // Upper right (x+1, y)
        double ur = 0.0;
        if (!get_pixel_double(ur, int_x + 1, int_y, band))
        {
            return false;
        }
        double ur_weight = frac_x * (1.0 - frac_y);

        // Lower left (x, y+1)
        double ll = 0.0;
        if (!get_pixel_double(ll, int_x, int_y + 1, band))
        {
            return false;
        }
        double ll_weight = (1.0 - frac_x) * frac_y;

        // Lower right (x+1, y+1)
        double lr = 0.0;
        if (!get_pixel_double(lr, int_x + 1, int_y + 1, band))
        {
            return false;
        }
        double lr_weight = frac_x * frac_y;

        // Perform bilinear interpolation
        value =
            ul * ul_weight + ur * ur_weight + ll * ll_weight + lr * lr_weight;
        return true;
    }

    // Provide a default implementation to get the value as a double and round
    // it
    bool ImageData::get_pixel_int(int &value,
                                  const int x,
                                  const int y,
                                  const int band) const
    {
        double result = 0.0;
        if (!get_pixel_double(result, x, y, band))
        {
            return false;
        }
        else
        {
            if (result > 0.0)
            {
                result += 0.5;
            }
            else if (result < 0.0)
            {
                result -= 0.5;
            }

            value = static_cast<int>(result);
            return true;
        }
    }

    // Provide a default implementation to get the value as a double and round
    // it
    bool ImageData::get_interpolated_pixel_int(int &value,
                                               const double x,
                                               const double y,
                                               const int band) const
    {
        double result = 0.0;
        if (!get_interpolated_pixel_double(result, x, y, band))
        {
            return false;
        }
        else
        {
            if (result > 0.0)
            {
                result += 0.5;
            }
            else if (result < 0.0)
            {
                result -= 0.5;
            }

            value = static_cast<int>(result);
            return true;
        }
    }

    void ImageData::set_interpolating(bool enable)
    {
        interpolate = enable;
    }

    int ImageData::get_interpolating() const
    {
        return interpolate;
    }

    double ImageData::get_edge_distance(const double x, const double y) const
    {
        const int width = get_width();
        const int height = get_height();
        if (width <= 0 || height <= 0)
        {
            // Unknown size - never feather
            return std::numeric_limits<double>::infinity();
        }

        const double distance = std::min(std::min(x, y),
                                         std::min((width - 1) - x,
                                                  (height - 1) - y));
        return std::max(0.0, distance);
    }

    void ImageData::set_alpha_band(int band)
    {
        alpha_band = band;
    }

    int ImageData::get_alpha_band() const
    {
        return alpha_band;
    }

    void ImageData::get_image_data(uint32_t *data_ptr,
                                   const std::string &color,
                                   uint8_t &pix_bytes) const
    {
        const int width = get_width();
        const int height = get_height();
        const int number_of_bands = get_bands();

        if (color == "BAYER" || color == "RGB")
        {
            struct pixel_t
            {
                uint8_t red;
                uint8_t green;
                uint8_t blue;
            } *data = reinterpret_cast<struct pixel_t *>(data_ptr);

            pix_bytes = 4;

            // If there are not enough bands to fill the bayer,
            // these values will default to 0.
            if (number_of_bands < 3)
            {
                fprintf(stderr,
                        "Requesting bayer image coloring with only "
                        "%d bands\n",
                        number_of_bands);
            }

            // Bayer format is RGGB with each field being 8 bits
            // Green takes up 2 fields
            for (int x = 0; x < width; x++)
            {
                for (int y = 0; y < height; y++)
                {
                    data[y * width + x].red = 0;
                    data[y * width + x].green = 0;
                    data[y * width + x].blue = 0;

                    // RED
                    if (number_of_bands >= 1)
                    {
                        int red_raw;
                        get_pixel_int(red_raw, x, y, 0);
                        data[y * width + x].red = red_raw & 0xff;
                    }

                    // GREEN_GREEN
                    if (number_of_bands >= 2)
                    {
                        int green_raw;
                        get_pixel_int(green_raw, x, y, 1);
                        data[y * width + x].green = green_raw & 0xff;
                    }

                    // BLUE
                    if (number_of_bands >= 3)
                    {
                        int blue_raw;
                        get_pixel_int(blue_raw, x, y, 2);
                        data[y * width + x].blue = blue_raw & 0xff;
                    }
                }
            }
        }
        else if (color == "PANCHROMATIC")
        {
            auto *data = reinterpret_cast<uint16_t *>(data_ptr);
            pix_bytes = 2;

            // We will average all bands (PANCHROMATIC)
            // Single band images will simply copy their data
            for (int x = 0; x < width; x++)
            {
                for (int y = 0; y < height; y++)
                {
                    int band_sum = 0;
                    for (int band = 0; band < number_of_bands; band++)
                    {
                        int pix_band = 0;
                        get_pixel_int(pix_band, x, y, band);

                        band_sum += pix_band;
                    }

                    int panchromatic_value = band_sum / number_of_bands;
                    data[y * width + x] = panchromatic_value;
                }
            }
        }
        else if (color == "BLUE" || color == "RED" || color == "GREEN")
        {
            auto *data = reinterpret_cast<uint16_t *>(data_ptr);
            pix_bytes = 2;

            int chosen_band = 0;
            if (number_of_bands == 2)
            {
                // This happens with intensity+alpha
                // images. The data band is '0'
            }
            else if (number_of_bands >= 3)
            {
                if (color == "RED")
                {
                    chosen_band = 0;
                }
                else if (color == "GREEN")
                {
                    chosen_band = 1;
                }
                else if (color == "BLUE")
                {
                    chosen_band = 2;
                }
            }

            // We assume the only band is BLUE
            for (int x = 0; x < width; x++)
            {
                for (int y = 0; y < height; y++)
                {
                    int pix_val;
                    get_pixel_int(pix_val, x, y, chosen_band);
                    data[y * width + x] = pix_val & 0xffff;
                }
            }
        }
        else
        {
            throw std::runtime_error("Invalid image color encoding: " + color);
        }
    }
}
