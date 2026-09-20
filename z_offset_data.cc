#include "z_offset_data.h"


namespace rsvp
{
    ZOffsetData::ZOffsetData(const std::shared_ptr<rsvp::ImageData> &inImg) :
        img(inImg)
    {
        if (inImg != nullptr)
        {
            const size_t bands = static_cast<size_t>(img->get_bands());
            scales.assign(bands, 1.0);
            offsets.assign(bands, 0.0);
        }
    }

    ZOffsetData::~ZOffsetData() = default;

    void
    ZOffsetData::set_offset_and_scale(int band, double _offset, double _scale)
    {
        if (has_band(band))
        {
            offsets[band] = _offset;
            scales[band] = _scale;
        }
    }

    bool ZOffsetData::get_pixel_double(double &value,
                                       const int x,
                                       const int y,
                                       const int band) const
    {
        double raw_result = 0.0;
        if (!has_band(band) || !img->get_pixel_double(raw_result, x, y, band))
        {
            return false;
        }

        value = (raw_result * scales[band]) + offsets[band];
        return true;
    }

    bool ZOffsetData::get_interpolated_pixel_double(double &value,
                                                    const double x,
                                                    const double y,
                                                    const int band) const
    {
        double raw_result = 0.0;
        if (!has_band(band) ||
            !img->get_interpolated_pixel_double(raw_result, x, y, band))
        {
            return false;
        }

        value = (raw_result * scales[band]) + offsets[band];
        return true;
    }

    bool ZOffsetData::get_clamped_pixel_double(double &value,
                                               double &weight,
                                               const double x,
                                               const double y,
                                               const int band) const
    {
        double raw_result = 0.0;
        if (!has_band(band) ||
            !img->get_clamped_pixel_double(raw_result, weight, x, y, band))
        {
            return false;
        }

        value = (raw_result * scales[band]) + offsets[band];
        return true;
    }

    bool ZOffsetData::get_pixel_int(int &value,
                                    const int x,
                                    const int y,
                                    const int band) const
    {
        int raw_result = 0;
        if (!has_band(band) || !img->get_pixel_int(raw_result, x, y, band))
        {
            return false;
        }

        value = round_to_int((raw_result * scales[band]) + offsets[band]);
        return true;
    }

    bool ZOffsetData::get_interpolated_pixel_int(int &value,
                                                 const double x,
                                                 const double y,
                                                 const int band) const
    {
        int raw_result = 0;
        if (!has_band(band) ||
            !img->get_interpolated_pixel_int(raw_result, x, y, band))
        {
            return false;
        }

        value = round_to_int((raw_result * scales[band]) + offsets[band]);
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

    void ZOffsetData::set_interpolating(bool enable)
    {
        ImageData::set_interpolating(enable);
        if (img != nullptr)
        {
            img->set_interpolating(enable);
        }
    }

    int ZOffsetData::get_interpolating() const
    {
        if (img != nullptr)
        {
            return img->get_interpolating();
        }
        else
        {
            return ImageData::get_interpolating();
        }
    }
}
