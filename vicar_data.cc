#include "vicar_data.h"

#include "platform.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>

#define TILE_SIZE 2048
#define TILE_DIM  16

static bool extract_vector(const std::string &array_str, double values[3])
{
    return sscanf(array_str.c_str(),
                  "(%lf,%lf,%lf)",
                  &values[0],
                  &values[1],
                  &values[2]) == 3;
}

namespace rsvp
{

    VicarData::VicarData() :
        NL(0),
        NS(0),
        NB(0),
        pds_bytes(0),
        lblsize(0),
        NBB(0),
        NLB(0),
        int_opposite_endian(false),
        real_opposite_endian(false)
    {
    }

    VicarData::VicarData(int samples,
                         int lines,
                         int bands,
                         DataFormat data_format) :
        VicarData()
    {
        this->NS = samples;
        this->NL = lines;
        this->NB = bands;
        this->format = data_format;

        // Create enough tile elements to store needed samples
        size_t num_tiles = bands * ((lines + (TILE_DIM - 1)) / TILE_DIM) * ((samples + (TILE_DIM - 1)) / TILE_DIM);

        // Align to 4KB, which is the standard page size on X86
        // With 8-byte doubles, this gives us 512 entries per tile, a TILE_DIM-by-TILE_DIM square
        this->pixel_data = static_cast<double *>(std::aligned_alloc(4096, TILE_SIZE * num_tiles));
    }

    VicarData::~VicarData()
    {
        // Free up allocated space in pixel_data
        if (this->pixel_data != nullptr)
        {
            free(this->pixel_data);
        }
    }

    void VicarData::set_labels(std::string _comments)
    {
        comments = std::move(_comments);

        std::stringstream comment_stream(comments);

        // Keep track of if we are in the Property labels
        std::string current_property;

        while (true)
        {
            // Strip off any leading whitespace
            comment_stream >> std::ws;

            // Tags cannot have a space, so search for the equal sign
            std::string tag;
            if (!std::getline(comment_stream, tag, '='))
            {
                break;
            }

            // Now, try to extract the value
            std::string value;

            if (comment_stream.peek() == '(')
            {
                // Multiple values may be stored inside parens
                if (!std::getline(comment_stream, value, ')'))
                {
                    break;
                }
                // `getline` consumed the closing paren
                value.push_back(')');
            }
            else if (comment_stream.peek() == '\'')
            {
                // Strings are enclosed by single quotes - tokenize on those.
                // Unfortunately, single quotes can be escaped by doubling them
                // up (`''`), so we need to keep going until we're done with
                // those.

                // We're going to strip off the wrapping single quotes, as well
                // as any doubled-up/escaping quotes, so make sure to lose the
                // first one.
                comment_stream.get();

                bool done_with_quotes = false;
                while (!done_with_quotes)
                {
                    std::string temp_value;
                    if (!std::getline(comment_stream, temp_value, '\''))
                    {
                        // We didn't find another quote - panic!
                        break;
                    }
                    value += temp_value;

                    done_with_quotes = (comment_stream.peek() != '\'');
                    if (!done_with_quotes)
                    {
                        // getline just consumed a quote, and the next
                        // character is a quote as well - this is actually an
                        // escaped part of the string. Put this one into the
                        // value and continue looking for the real terminating
                        // quote.
                        value.push_back(
                            static_cast<char>(comment_stream.get()));
                    }
                }

                if (!done_with_quotes)
                {
                    // We escaped the loop above without our trigger condition
                    // - panic!
                    break;
                }
            }
            else if (!(comment_stream >> value))
            {
                // Boring version - just tokenize on whitespace.
                break;
            }

            if (tag == "TASK")
            {
                // The only labels we care about are the system labels, which
                // must come first, and the property labels, which must come
                // next. If we find the start of the task labels (keyword
                // "TASK"), we just abort.
                break;
            }
            else if (tag == "PROPERTY")
            {
                // We're into the property labels, so assign the current
                // property name
                current_property = value;
                label_values[current_property] =
                    std::unordered_map<std::string, std::string>();
            }
            else if (!current_property.empty())
            {
                // We're in the property labels - save this for later access.
                label_values[current_property][tag] = value;
            }
            else
            {
                // We're still in the system labels
                if (tag == "RECSIZE")
                {
                    recsize = std::stoi(value);
                }
                else if (tag == "LBLSIZE")
                {
                    lblsize = std::stoi(value);
                }
                else if (tag == "FORMAT")
                {
                    if (value == "'BYTE'")
                    {
                        format = BYTE;
                    }
                    else if (value == "HALF" || value == "WORD")
                    {
                        format = HALF;
                    }
                    else if (value == "FULL" || value == "LONG")
                    {
                        format = FULL;
                    }
                    else if (value == "REAL")
                    {
                        format = REAL;
                    }
                    else if (value == "DOUB")
                    {
                        format = DOUB;
                    }
                    else if (value == "COMP" || value == "COMPLEX")
                    {
                        format = COMP;
                    }
                }
                else if (tag == "ORG")
                {
                    if (value == "BSQ")
                    {
                        org = BSQ;
                    }
                    else if (value == "BIL")
                    {
                        org = BIL;
                    }
                    else if (value == "BIP")
                    {
                        org = BIP;
                    }
                }
                else if (tag == "INTFMT")
                {
                    if (value == "LOW")
                    {
                        intfmt = LOW;
                    }
                    else if (value == "HIGH")
                    {
                        intfmt = HIGH;
                    }
                }
                else if (tag == "REALFMT")
                {
                    if (value == "IEEE")
                    {
                        realfmt = IEEE;
                    }
                    else if (value == "RIEEE")
                    {
                        realfmt = RIEEE;
                    }
                    else if (value == "VAX")
                    {
                        realfmt = VAX;
                    }
                }
                else if (tag == "NL")
                {
                    NL = std::stoi(value);
                }
                else if (tag == "NS")
                {
                    NS = std::stoi(value);
                }
                else if (tag == "NB")
                {
                    NB = std::stoi(value);
                }
                else if (tag == "NBB")
                {
                    NBB = std::stoi(value);
                }
                else if (tag == "NLB")
                {
                    NLB = std::stoi(value);
                }
                else
                {
                    std::array<std::string_view, 6> remaining_labels {
                        "N1", "N2", "N3", "DIM", "TYPE", "BUFSIZ"};
                    if (std::find(remaining_labels.begin(),
                                  remaining_labels.end(),
                                  tag) == remaining_labels.end())
                    {
                        unassociated_labels[tag] = value;
                    }
                }
            }
        }

        if (NLB < 0)
        {
            NLB = 0;
        }

        if (NBB < 0)
        {
            NBB = 0;
        }

        switch (org)
        {
        case BSQ:
            N1 = NS;
            N2 = NL;
            N3 = NB;
            break;
        case BIL:
            N1 = NS;
            N2 = NB;
            N3 = NL;
            break;
        case BIP:
            N1 = NB;
            N2 = NS;
            N3 = NL;
            break;
        }

        // RIEEE is little-endian, and IEEE and VAX are big-endian.
        bool real_little_endian = realfmt == RIEEE;

        // LOW is little-endian, HIGH is not
        bool int_little_endian = intfmt == LOW;

        // Cast a two- or four-byte integer '1' (0x01 or 0x0001) value to a
        // one-byte char; if that char has the bit set (0x1) then the host
        // is by definition little-endian.
        int num = 1;
        bool host_little_endian = (*reinterpret_cast<char *>(&num) == 1);

        real_opposite_endian = host_little_endian != real_little_endian;
        int_opposite_endian = host_little_endian != int_little_endian;
    }

    bool VicarData::get_pixel_double(double &value,
                                     const int sample,
                                     const int line,
                                     const int band) const
    {
        if (line < 0 || sample < 0 || band < 0)
        {
            return false;
        }

        if (line >= NL || sample >= NS || band >= NB)
        {
            return false;
        }

        // pixel_data is always organized as BSQ (no interlacing)
        // Each tile is a TILE_DIM-by-TILE_DIM pixel image
        size_t tile_rows = (this->NL + (TILE_DIM - 1)) / TILE_DIM, tile_cols = (this->NS + (TILE_DIM - 1)) / TILE_DIM;
        size_t target_tile = (band * (tile_rows * tile_cols)) + ((line / TILE_DIM) * tile_cols) + (sample / TILE_DIM);
        double *tile = this->pixel_data + ((TILE_SIZE / sizeof(double)) * target_tile);
        // Grab the value from the tile that contains it
        value = *(tile + (((line % TILE_DIM) * TILE_DIM) + (sample % TILE_DIM)));

        return true;
    }

    bool VicarData::set_pixel_double(const double value,
                                     const int sample,
                                     const int line,
                                     const int band)
    {
        if (line < 0 || sample < 0 || band < 0)
        {
            return false;
        }

        if (line >= NL || sample >= NS || band >= NB)
        {
            return false;
        }

        // Each tile is a TILE_DIM-by-TILE_DIM pixel image, in BSQ order
        size_t tile_rows = (this->NL + (TILE_DIM - 1)) / TILE_DIM, tile_cols = (this->NS + (TILE_DIM - 1)) / TILE_DIM;
        size_t target_tile = (band * (tile_rows * tile_cols)) + ((line / TILE_DIM) * tile_cols) + (sample / TILE_DIM);
        double *tile = this->pixel_data + ((TILE_SIZE / sizeof(double)) * target_tile);
        // Grab the value from the tile that contains it
        *(tile + (((line % TILE_DIM) * TILE_DIM) + (sample % TILE_DIM))) = value;

        return true;
    }

    std::shared_ptr<VicarData>
    VicarData::read_vicarfile(const std::string &path)
    {
        // This is specific to .ht files
        std::ifstream ht_file(path, std::ios::binary);

        // If we failed to open the file, abort
        if (!ht_file.is_open())
        {
            throw std::runtime_error("Unable to open Vicar data file: " +
                                     path);
        }

        // Get the first space-separated token in the file
        std::string label_tag;
        ht_file >> label_tag;

        int pds_label_bytes = 0;

        // If this is PDS (or ODL, the ops PDS-like filetype), try to skip
        // over the header
        if (label_tag.find("ODL_VERSION_ID") == 0 ||
            label_tag.find("PDS_VERSION_ID") == 0)
        {
            std::string label_line;

            int record_bytes = 0;
            int label_records = 0;

            while (std::getline(ht_file, label_line))
            {
                size_t equal_pos = label_line.find('=');

                // We're tokenizing on spaces, so it's always possible to
                // break up a quoted string by accident - just ignore those
                // for our use case
                if (equal_pos == std::string::npos)
                {
                    continue;
                }

                std::string tag = label_line.substr(0, equal_pos);
                std::string value =
                    label_line.substr(equal_pos + 1, std::string::npos);

                if (tag.find("RECORD_BYTES") != std::string::npos)
                {
                    record_bytes = std::stoi(value);
                }
                else if (tag.find("LABEL_RECORDS") != std::string::npos)
                {
                    label_records = std::stoi(value);
                }

                if (record_bytes != 0 && label_records != 0)
                {
                    break;
                }
            }

            pds_label_bytes = record_bytes * label_records;

            // If we didn't find the size of the PDS header, abort
            if (pds_label_bytes == 0)
            {
                throw std::runtime_error(
                    path +
                    ": PDS file does not have `RECORD_BYTES` or "
                    "`LABEL_RECORDS` labels!");
            }

            // Seek the end of the PDS label and try to re-read the first
            // vicar label
            ht_file.clear();
            ht_file.seekg(label_records * record_bytes, std::ios::beg);

            ht_file >> label_tag;
        }

        // Check if this is a valid Vicar file
        if (label_tag.find("LBLSIZE=") != 0)
        {
            throw std::runtime_error(
                path + ": First characters of vicar file were `" + label_tag +
                "`, not `LBLSIZE=` or `ODL_VERSION_ID` or "
                "`PDS_VERSION_ID`!");
        }

        // Extract the size of the header labels
        std::string label_size_string = label_tag.substr(8, std::string::npos);
        const long label_size = std::stol(label_size_string);

        // Initialize an empty string large enough to hold the labels
        std::string comments(static_cast<size_t>(label_size), '\0');
        comments.clear();

        // Reset the stream back to the beginning of the vicar labels for
        // ease of ingestion and read it into the string
        ht_file.clear();
        ht_file.seekg(pds_label_bytes, std::ios::beg);

        for (int i = 0; i < label_size; i++)
        {
            comments.push_back(static_cast<char>(ht_file.get()));
        }

        // Strip off all of the trailing null bytes - they interfere with
        // the EOL label parsing
        comments = comments.substr(0, comments.find_first_of('\0'));

        // Create a VicarData to be returned and have it parse its own
        // comments
        std::shared_ptr<VicarData> result(new VicarData());
        result->filepath = path;
        result->set_labels(comments);

        result->pds_bytes = pds_label_bytes;

        // The data actually begins after both of the PDS and vicar labels
        // and any binary label lines, but mmap can only map data on
        // page-size boundaries (and pages are typically larger than
        // LBLSIZE). We make sure to get the labels, the binary labels, and
        // all of the data.
        int map_offset = result->pds_bytes +         // PDS label
            result->lblsize +                        // Vicar label
            result->get_record_size() * result->NLB; // Binary header

        int data_length = result->get_n1() *          // Logical lines
            (result->get_binary_prefix_byte_count() + // Label bytes per line
             result->get_n2() *                       // Logical samples
                 result->get_n3() *                   // Logical bands
                 result->get_pixel_byte_count());     // Bytes per pixel

        // Append any EOL labels
        if (comments.find("EOL=1") != std::string::npos)
        {
            ht_file.seekg(map_offset + data_length, std::ios::beg);

            // Read everything until the first space
            std::string eol_lblsize;
            if (!std::getline(ht_file, eol_lblsize, ' '))
            {
                throw std::runtime_error("Could not read line from file: " +
                                         path);
            }

            size_t equal_pos = eol_lblsize.find('=');

            std::string tag = eol_lblsize.substr(0, equal_pos);
            std::string value =
                eol_lblsize.substr(equal_pos + 1, std::string::npos);

            if (tag != "LBLSIZE")
            {
                throw std::runtime_error(path + ": Tag '" + tag +
                                         "'does not match 'LBLSIZE'");
            }

            auto eol_label_size = static_cast<unsigned int>(std::stoi(value));
            // Subtract off the length of the string we've already read, as
            // well as the space delimiter
            eol_label_size -= eol_lblsize.size() + 1;

            // Append the EOL labels to the comments
            comments.reserve(comments.size() + eol_label_size);

            for (unsigned int i = 0; i < eol_label_size; i++)
            {
                comments.push_back(static_cast<char>(ht_file.get()));
            }

            // Parse the comments again
            result->set_labels(comments);
        }

        // Read the pixel data
        ht_file.seekg(map_offset, std::ios::beg);

        auto data_array = std::unique_ptr<uint8_t[]>(
            new uint8_t[static_cast<size_t>(data_length)]);
        ht_file.read(reinterpret_cast<char *>(data_array.get()), data_length);

        // We're done with reading the file directly
        ht_file.close();

        // Process the pixel data in the data_array into the pixel array
        // BSQ  n1 = sample
        //      n2 = line
        //      n3 = band
        // BIL  n1 = sample
        //      n2 = band
        //      n3 = line
        // BIP  n1 = band
        //      n2 = sample
        //      n3 = line

        int fmt_size = result->get_pixel_byte_count();

        // Create enough tile elements to store needed samples
        size_t num_tiles = result->NB * ((result->NL + (TILE_DIM - 1)) / TILE_DIM) * ((result->NS + (TILE_DIM - 1)) / TILE_DIM);

        // We don't use shared_ptr here as it would double memory usage
        // Align to 4KB, which is the standard page size on X86
        // With 8-byte doubles, this gives us 512 entries per tile, a TILE_DIM-by-TILE_DIM square            
        result->pixel_data = static_cast<double *>(std::aligned_alloc(4096, TILE_SIZE * num_tiles));

        int raw_data_is_int =
            (result->format == BYTE || result->format == HALF ||
             result->format == WORD || result->format == FULL ||
             result->format == LONG);

        int perform_swap = (raw_data_is_int && result->int_opposite_endian) ||
            (!raw_data_is_int && result->real_opposite_endian);

        for (int n3 = 0; n3 < result->N3; n3++)
        {
            int n3_offset = n3 * result->NBB + // Prior lines' prefixes
                n3 * result->N2 * result->N1 * fmt_size + // Prior lines' data
                result->NBB; // Current line's prefix
            for (int n2 = 0; n2 < result->N2; n2++)
            {
                int n2_offset = n3_offset +
                    n2 * result->N1 * fmt_size; // This line's prior samples
                for (int n1 = 0; n1 < result->N1; n1++)
                {
                    int offset =
                        n2_offset + n1 * fmt_size; // This sample's prior bytes

                    const unsigned char *p =
                        &data_array[static_cast<size_t>(offset)];
                    union
                    {
                        char i8;
                        short i16;
                        int i32;
                        float f32;
                        double f64;
                        unsigned char buffer[8];
                    } integer_union {};

                    double value;
                    if (perform_swap)
                    {
                        rsvp::byte_swap(integer_union.buffer, p, fmt_size);
                    }
                    else
                    {
                        memcpy(
                            &integer_union, p, static_cast<size_t>(fmt_size));
                    }

                    // Cast type to double
                    switch (result->format)
                    {
                    case BYTE:
                        value = static_cast<double>(integer_union.i8);
                        break;
                    case HALF:
                        value = static_cast<double>(integer_union.i16);
                        break;
                    case FULL:
                        value = static_cast<double>(integer_union.i32);
                        break;
                    case REAL:
                        value = static_cast<double>(integer_union.f32);
                        break;
                    case DOUB:
                        value = integer_union.f64;
                        break;
                    default:
                        throw std::runtime_error(
                            path + ": Unhandled format type" +
                            std::to_string(result->format));
                    }

                    size_t tile_rows = (result->NL + (TILE_DIM - 1)) / TILE_DIM, tile_cols = (result->NS + (TILE_DIM - 1)) / TILE_DIM;
                    size_t target_tile = 0;
                    double *tile = nullptr;

                    switch (result->org)
                    {
                    case BSQ:
                        // No reshuffling needed
                        // This is fastest
                        // n3 = band
                        // n2 = line
                        // n1 = sample
                        target_tile = (n3 * (tile_rows * tile_cols)) + ((n2 / TILE_DIM) * tile_cols) + (n1 / TILE_DIM);
                        tile = result->pixel_data + ((TILE_SIZE / sizeof(double)) * target_tile);
                        *(tile + (((n2 % TILE_DIM) * TILE_DIM) + (n1 % TILE_DIM))) = value;
                        break;
                    case BIL:
                        // n2 = band
                        // n3 = line
                        // n1 = sample
                        target_tile = (n2 * (tile_rows * tile_cols)) + ((n3 / TILE_DIM) * tile_cols) + (n1 / TILE_DIM);
                        tile = result->pixel_data + ((TILE_SIZE / sizeof(double)) * target_tile);
                        *(tile + (((n3 % TILE_DIM) * TILE_DIM) + (n1 % TILE_DIM))) = value;
                        break;
                    case BIP:
                        // n1 = band
                        // n3 = line
                        // n2 = sample
                        target_tile = (n1 * (tile_rows * tile_cols)) + ((n3 / TILE_DIM) * tile_cols) + (n2 / TILE_DIM);
                        tile = result->pixel_data + ((TILE_SIZE / sizeof(double)) * target_tile);
                        *(tile + (((n3 % TILE_DIM) * TILE_DIM) + (n2 % TILE_DIM))) = value;
                        break;
                    }
                }
            }
        }

        return result;
    }

    bool is_number(const std::string &n)
    {
        std::regex number_regex(R"([+-]?\d+(?:\.\d*)?)");
        return std::regex_match(n, number_regex);
    }

    void VicarData::write_vicarfile(const std::string &path) const
    {
        std::stringstream labels;
        // system labels
        // LBLSIZE is calculated after the result
        labels << "LBLSIZE= ";

        labels << "FORMAT='";
        switch (format)
        {
        case BYTE:
            labels << "BYTE";
            break;
        case HALF:
            labels << "HALF";
            break;
        case FULL:
            labels << "FULL";
            break;
        case REAL:
            labels << "REAL";
            break;
        case DOUB:
            labels << "DOUB";
            break;
        case COMP:
            labels << "COMP";
            break;
        }
        labels << "' ";

        // image_data assumes type is image
        labels << "TYPE='IMAGE' ";

        const auto rec_size = NS * get_pixel_byte_count();
        labels << "BUFSIZ=" << rec_size << ' ';

        labels << "DIM=3 ";

        labels << "EOL=0 ";

        labels << "RECSIZE=" << rec_size << ' ';

        labels << "ORG='BSQ' ";

        labels << "NL=" << NL << ' ';
        labels << "NS=" << NS << ' ';
        labels << "NB=" << NB << ' ';
        labels << "N1=" << NS << ' ';
        labels << "N2=" << NL << ' ';
        labels << "N3=" << NB << ' ';
        labels << "N4=0 ";

        labels << "NBB=0 ";
        labels << "NLB=0 ";

        // maybe I should actually check this
        labels << "HOST='X64-64-LINX' ";

        labels << "INTFMT='LOW' ";

        labels << "REALFMT='RIEEE' ";

        labels << "BHOST='X64-64-LINX' ";
        labels << "BINTFMT='LOW' ";
        labels << "BREALFMT='RIEEE' ";
        labels << "BLTYPE='' ";

        for (const auto &label : unassociated_labels)
        {
            labels << label.first << "=";
            if (is_number(label.second))
            {
                labels << label.second;
            }
            else
            {
                labels << "'" << label.second << "'";
            }
            labels << " ";
        }

        for (const auto &group : label_values)
        {
            labels << "PROPERTY='" << group.first << "' ";
            for (const auto &label : group.second)
            {
                labels << label.first << "=";
                if (is_number(label.second))
                {
                    labels << label.second;
                }
                else
                {
                    labels << "'" << label.second << "'";
                }
                labels << " ";
            }
        }

        std::string labels_str = labels.str();
        size_t label_len = labels_str.length();
        size_t lblsize_len = std::to_string(label_len).length();
        label_len += lblsize_len;
        label_len =
            label_len - lblsize_len + std::to_string(label_len).length();

        labels_str.insert(8, std::to_string(label_len));

        std::ofstream outfile(path, std::ofstream::binary);
        outfile << labels_str;

        for (int b = 0; b < get_bands(); ++b)
        {
            for (int y = 0; y < get_height(); ++y)
            {
                for (int x = 0; x < get_width(); ++x)
                {
                    double value;
                    get_pixel_double(value, x, y, b);

                    union
                    {
                        unsigned char u8;
                        unsigned short u16;
                        unsigned int u32;
                        float f32;
                        double f64;
                        unsigned char buffer[8];
                    } integer_union {};

                    switch (format)
                    {
                    case BYTE:
                        integer_union.u8 = static_cast<uint8_t>(value);
                        break;
                    case HALF:
                        integer_union.u16 = static_cast<uint16_t>(value);
                        break;
                    case FULL:
                        integer_union.u32 = static_cast<uint32_t>(value);
                        break;
                    case REAL:
                        integer_union.f32 = static_cast<float>(value);
                        break;
                    case DOUB:
                        integer_union.f64 = value;
                        break;
                    default:
                        throw std::runtime_error {
                            "Unable to handle complex values!"};
                    }

                    outfile.write(
                        reinterpret_cast<const char *>(integer_union.buffer),
                        get_pixel_byte_count());
                }
            }
        }
    }

    bool VicarData::get_label_property(const std::string &property_group,
                                       const std::string &property_name,
                                       std::string &value) const
    {
        auto group_search = label_values.find(property_group);
        if (group_search != label_values.end())
        {
            auto name_search = group_search->second.find(property_name);
            if (name_search != group_search->second.end())
            {
                value.assign(name_search->second);
                return true;
            }
        }
        return false;
    }

    void VicarData::set_label_unassociated_property(
        const std::string &property_name, const std::string &value)
    {
        unassociated_labels.insert({property_name, value});
    }

    bool
    VicarData::get_label_indexed_property(const std::string &property_group,
                                          const std::string &property_name,
                                          unsigned int index,
                                          std::string &value) const
    {

        // Get the raw array string
        if (!get_label_property(property_group, property_name, value))
        {
            return false;
        }

        // The first non-blank character of the value must be '(' and the
        // last non-blank character must be ')'
        std::string::size_type open_paren =
            value.find_first_not_of(" \v\f\n\r\t");
        std::string::size_type close_paren =
            value.find_last_not_of(" \v\f\n\r\t");

        if (open_paren == std::string::npos ||
            close_paren == std::string::npos || value.at(open_paren) != '(' ||
            value.at(close_paren) != ')')
        {
            return false;
        }

        // Set up a stringstream for the text between the parens
        std::stringstream array_stream(
            value.substr(open_paren + 1, close_paren - open_paren - 1));

        for (unsigned int i = 0; i <= index; i++)
        {
            // Strip off any leading whitespace
            array_stream >> std::ws;

            // Extract the next comma-separated token, returning failure if
            // something goes wrong
            if (!std::getline(array_stream, value, ','))
            {
                return false;
            }
        }

        return true;
    }

    bool VicarData::get_camera_cahv_frame(std::string &frame) const
    {
        return get_label_property(
            "GEOMETRIC_CAMERA_MODEL", "REFERENCE_COORD_SYSTEM_NAME", frame);
    }

    bool VicarData::get_camera_c(double camera_c[3]) const
    {
        std::string raw_value;

        return get_label_property(
                   "GEOMETRIC_CAMERA_MODEL", "MODEL_COMPONENT_1", raw_value) &&
            extract_vector(raw_value, camera_c);
    }

    bool VicarData::get_camera_a(double camera_a[3]) const
    {
        std::string raw_value;

        return get_label_property(
                   "GEOMETRIC_CAMERA_MODEL", "MODEL_COMPONENT_2", raw_value) &&
            extract_vector(raw_value, camera_a);
    }

    bool VicarData::get_camera_h(double camera_h[3]) const
    {
        std::string raw_value;

        return get_label_property(
                   "GEOMETRIC_CAMERA_MODEL", "MODEL_COMPONENT_3", raw_value) &&
            extract_vector(raw_value, camera_h);
    }

    bool VicarData::get_camera_v(double camera_v[3]) const
    {
        std::string raw_value;

        return get_label_property(
                   "GEOMETRIC_CAMERA_MODEL", "MODEL_COMPONENT_4", raw_value) &&
            extract_vector(raw_value, camera_v);
    }

    bool VicarData::get_camera_o(double camera_o[3]) const
    {
        std::string raw_value;

        return get_label_property(
                   "GEOMETRIC_CAMERA_MODEL", "MODEL_COMPONENT_5", raw_value) &&
            extract_vector(raw_value, camera_o);
    }

    bool VicarData::get_camera_r(double camera_r[3]) const
    {
        std::string raw_value;

        return get_label_property(
                   "GEOMETRIC_CAMERA_MODEL", "MODEL_COMPONENT_6", raw_value) &&
            extract_vector(raw_value, camera_r);
    }

    bool VicarData::get_camera_e(double camera_e[3]) const
    {
        std::string raw_value;

        return get_label_property(
                   "GEOMETRIC_CAMERA_MODEL", "MODEL_COMPONENT_7", raw_value) &&
            extract_vector(raw_value, camera_e);
    }
} // namespace rsvp
