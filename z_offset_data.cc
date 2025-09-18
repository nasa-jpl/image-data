#include "z_offset_data.h"
#include <list>


namespace rsvp
{
    ZOffsetData::ZOffsetData(const std::shared_ptr<rsvp::ImageData> &inImg) :
        img(inImg)
    {
        if (inImg != nullptr)
        {
            for (int i = 0; i < img->get_bands(); i++)
            {
                scales.push_back(1.0);
                offsets.push_back(0.0);
            }
        }
    }

    ZOffsetData::~ZOffsetData() = default;

    void
    ZOffsetData::set_offset_and_scale(int band, double _offset, double _scale)
    {
        if (band >= 0 && band < get_bands())
        {
            offsets.at(band) = _offset;
            scales.at(band) = _scale;
        }
    }

    bool ZOffsetData::get_pixel_double(double &value,
                                       const int x,
                                       const int y,
                                       const int band) const
    {
        if (band < 0 || band >= get_bands())
        {
            return false;
        }

        double raw_result = 0.0;
        if (!img->get_pixel_double(raw_result, x, y, band))
        {
            return false;
        }

        value = (raw_result * scales.at(band)) + offsets.at(band);
        return true;
    }

    bool ZOffsetData::get_interpolated_pixel_double(double &value,
                                                    const double x,
                                                    const double y,
                                                    const int band) const
    {
        if (band < 0 || band >= get_bands())
        {
            return false;
        }

        double raw_result = 0.0;
        if (!img->get_interpolated_pixel_double(raw_result, x, y, band))
        {
            return false;
        }

        value = (raw_result * scales.at(band)) + offsets.at(band);
        return true;
    }

    bool ZOffsetData::get_pixel_int(int &value,
                                    const int x,
                                    const int y,
                                    const int band) const
    {
        if (band < 0 || band >= get_bands())
        {
            return false;
        }

        int raw_result = 0;
        if (!img->get_pixel_int(raw_result, x, y, band))
        {
            return false;
        }

        double scaled_value =
            (raw_result * scales.at(band)) + offsets.at(band);

        if (scaled_value > 0)
        {
            value = static_cast<int>(scaled_value + 0.5);
        }
        else
        {
            value = static_cast<int>(scaled_value - 0.5);
        }

        return true;
    }

    bool ZOffsetData::get_interpolated_pixel_int(int &value,
                                                 const double x,
                                                 const double y,
                                                 const int band) const
    {
        if (band < 0 || band >= get_bands())
        {
            return false;
        }

        int raw_result = 0;
        if (!img->get_interpolated_pixel_int(raw_result, x, y, band))
        {
            return false;
        }

        double scaled_value =
            (raw_result * scales.at(band)) + offsets.at(band);

        if (scaled_value > 0)
        {
            value = static_cast<int>(scaled_value + 0.5);
        }
        else
        {
            value = static_cast<int>(scaled_value - 0.5);
        }

        return true;
    }

    int ZOffsetData::get_bands() const
    {
        if (img != nullptr)
        {
            return img->get_bands();
        }
        else
        {
            return 0;
        }
    }

    void ZOffsetData::set_alpha_band(int band)
    {
        ImageData::set_alpha_band(band);
        if (img != nullptr)
        {
            img->set_alpha_band(band);
        }
    }

    int ZOffsetData::get_alpha_band() const
    {
        if (img != nullptr)
        {
            return img->get_alpha_band();
        }
        else
        {
            return ImageData::get_alpha_band();
        }
    }
}
