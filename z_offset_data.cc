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
        if (band < 0)
        {
            return;
        }

        if (!has_band(band))
        {
            // Either the stored image has gained bands since the tables were
            // sized, or this is a band it does not have. Only the first grows
            // the tables: `band_in_range` takes a band with an entry to be
            // one the stored image has, so an entry must never be made for a
            // band it does not.
            if (band >= get_bands())
            {
                return;
            }

            const size_t bands = static_cast<size_t>(get_bands());
            scales.resize(bands, 1.0);
            offsets.resize(bands, 0.0);
        }

        offsets[band] = _offset;
        scales[band] = _scale;
    }

    bool ZOffsetData::get_pixel_double(double &value,
                                       const int x,
                                       const int y,
                                       const int band) const
    {
        double raw_result = 0.0;
        if (!img || !band_in_range(band) ||
            !img->get_pixel_double(raw_result, x, y, band))
        {
            return false;
        }

        value = transform(raw_result, band);
        return true;
    }

    bool ZOffsetData::get_interpolated_pixel_double(double &value,
                                                    const double x,
                                                    const double y,
                                                    const int band) const
    {
        double raw_result = 0.0;
        if (!img || !band_in_range(band) ||
            !img->get_interpolated_pixel_double(raw_result, x, y, band))
        {
            return false;
        }

        value = transform(raw_result, band);
        return true;
    }

    bool ZOffsetData::get_interpolated_bands_double(double *values,
                                                    const int *bands,
                                                    const int count,
                                                    const double x,
                                                    const double y) const
    {
        if (!img)
        {
            return false;
        }

        for (int i = 0; i < count; i++)
        {
            if (!band_in_range(bands[i]))
            {
                return false;
            }
        }

        if (!img->get_interpolated_bands_double(values, bands, count, x, y))
        {
            return false;
        }

        for (int i = 0; i < count; i++)
        {
            values[i] = transform(values[i], bands[i]);
        }

        return true;
    }

    bool ZOffsetData::get_clamped_pixel_double(double &value,
                                               double &weight,
                                               const double x,
                                               const double y,
                                               const int band) const
    {
        double raw_result = 0.0;
        if (!img || !band_in_range(band) ||
            !img->get_clamped_pixel_double(raw_result, weight, x, y, band))
        {
            return false;
        }

        value = transform(raw_result, band);
        return true;
    }

    bool ZOffsetData::get_pixel_int(int &value,
                                    const int x,
                                    const int y,
                                    const int band) const
    {
        int raw_result = 0;
        if (!img || !band_in_range(band) ||
            !img->get_pixel_int(raw_result, x, y, band))
        {
            return false;
        }

        value = round_to_int(transform(raw_result, band));
        return true;
    }

    bool ZOffsetData::get_interpolated_pixel_int(int &value,
                                                 const double x,
                                                 const double y,
                                                 const int band) const
    {
        int raw_result = 0;
        if (!img || !band_in_range(band) ||
            !img->get_interpolated_pixel_int(raw_result, x, y, band))
        {
            return false;
        }

        value = round_to_int(transform(raw_result, band));
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
