#include "vicar_data.h"

#include "platform.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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

    namespace
    {
        // Decodes `count` pixels of one raw type into doubles, `stride`
        // doubles apart. Chosen once per file rather than switching on the
        // format and byte order for every pixel.
        using Decoder = void (*)(const uint8_t *source,
                                 double *destination,
                                 int count,
                                 size_t stride);

        template <typename T, bool swap>
        void decode_run(const uint8_t *source,
                        double *destination,
                        const int count,
                        const size_t stride)
        {
            for (int i = 0; i < count; i++)
            {
                // Swapping the raw bytes and then copying them into a T keeps
                // this free of type punning, which matters now that it is
                // inlined into the innermost loop.
                unsigned char bytes[sizeof(T)];
                if constexpr (swap)
                {
                    byte_swap(bytes, source, sizeof(T));
                }
                else
                {
                    memcpy(bytes, source, sizeof(T));
                }

                T raw;
                memcpy(&raw, bytes, sizeof(T));
                *destination = static_cast<double>(raw);

                source += sizeof(T);
                destination += stride;
            }
        }

        template <typename T>
        Decoder decoder(const bool swap)
        {
            return swap ? &decode_run<T, true> : &decode_run<T, false>;
        }

        // The decoder for a format, or null if the format cannot be decoded.
        // VICAR's BYTE is unsigned; the other integer formats are signed.
        Decoder decoder_for(const VicarData::DataFormat format,
                            const bool swap)
        {
            switch (format)
            {
            case VicarData::BYTE:
                return decoder<uint8_t>(false);
            case VicarData::HALF:
                return decoder<int16_t>(swap);
            case VicarData::FULL:
                return decoder<int32_t>(swap);
            case VicarData::REAL:
                return decoder<float>(swap);
            case VicarData::DOUB:
                return decoder<double>(swap);
            case VicarData::COMP:
                break;
            }

            return nullptr;
        }

        // Encodes `count` doubles into one raw type, in the host byte order.
        // The raw types mirror the decoder's. Converting a double to an
        // integer type it does not fit is undefined, and comes out differently
        // on x86-64 and arm64, so integer types saturate first: NaN to zero,
        // everything else to the nearest end of the type's range.
        using Encoder =
            void (*)(const double *source, uint8_t *destination, int count);

        template <typename T>
        T to_raw(const double value)
        {
            if constexpr (std::is_integral<T>::value)
            {
                if (std::isnan(value))
                {
                    return 0;
                }

                return static_cast<T>(std::clamp(
                    value,
                    static_cast<double>(std::numeric_limits<T>::lowest()),
                    static_cast<double>(std::numeric_limits<T>::max())));
            }
            else
            {
                return static_cast<T>(value);
            }
        }

        template <typename T>
        void encode_run(const double *source,
                        uint8_t *destination,
                        const int count)
        {
            for (int i = 0; i < count; i++)
            {
                const T raw = to_raw<T>(source[i]);
                memcpy(destination, &raw, sizeof(T));
                destination += sizeof(T);
            }
        }

        Encoder encoder_for(const VicarData::DataFormat format)
        {
            switch (format)
            {
            case VicarData::BYTE:
                return &encode_run<uint8_t>;
            case VicarData::HALF:
                return &encode_run<int16_t>;
            case VicarData::FULL:
                return &encode_run<int32_t>;
            case VicarData::REAL:
                return &encode_run<float>;
            case VicarData::DOUB:
                return &encode_run<double>;
            case VicarData::COMP:
                break;
            }

            return nullptr;
        }

        // Read `count` bytes from `file` onto the end of `text`
        void append_from_file(std::string &text,
                              std::ifstream &file,
                              const size_t count)
        {
            const size_t old_size = text.size();
            text.resize(old_size + count, '\0');
            file.read(&text[old_size], static_cast<std::streamsize>(count));
        }

        // The system labels write_vicarfile works out for itself. set_labels
        // never keeps one of these as an unassociated label, so the writer
        // cannot repeat one from the labels read in: a repeated label
        // overrides the one written, and a repeated `EOL=1` sends the reader
        // looking for end-of-line labels that were never written.
        //
        // Every `TAG=` the writer emits ahead of the unassociated labels must
        // be here, and vice versa.
        const std::array<std::string_view, 24> system_labels {
            "LBLSIZE", "FORMAT",  "TYPE",  "BUFSIZ",  "DIM",      "EOL",
            "RECSIZE", "ORG",     "NL",    "NS",      "NB",       "N1",
            "N2",      "N3",      "N4",    "NBB",     "NLB",      "HOST",
            "INTFMT",  "REALFMT", "BHOST", "BINTFMT", "BREALFMT", "BLTYPE"};

        bool is_system_label(const std::string &tag)
        {
            return std::find(system_labels.begin(),
                             system_labels.end(),
                             tag) != system_labels.end();
        }
    }

    VicarData::VicarData() = default;

    VicarData::VicarData(int samples,
                         int lines,
                         int bands,
                         DataFormat data_format) :
        NL(lines),
        NS(samples),
        NB(bands),
        pixel_data(new double[static_cast<size_t>(bands) * lines * samples]),
        format(data_format)
    {
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
                else if (!is_system_label(tag))
                {
                    // Anything the writer does not work out for itself is
                    // kept, to be written back as it was read
                    unassociated_labels[tag] = value;
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

        pixel_data[pixel_index(sample, line, band)] = value;
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

        // Reset the stream back to the beginning of the vicar labels for
        // ease of ingestion and read it into a string
        ht_file.clear();
        ht_file.seekg(pds_label_bytes, std::ios::beg);

        std::string comments;
        append_from_file(comments, ht_file, static_cast<size_t>(label_size));

        // Strip off all of the trailing null bytes - they interfere with
        // the EOL label parsing
        comments.resize(std::min(comments.find('\0'), comments.size()));

        // Create a VicarData to be returned and have it parse its own
        // comments
        std::shared_ptr<VicarData> result(new VicarData());
        result->set_labels(comments);

        // The data actually begins after both of the PDS and vicar labels
        // and any binary label lines.
        const std::streamoff data_offset = pds_label_bytes + // PDS label
            result->lblsize +                                // Vicar label
            static_cast<std::streamoff>(result->get_record_size()) *
                result->NLB; // Binary header

        const size_t data_length = static_cast<size_t>(result->get_n1()) *
            (result->get_binary_prefix_byte_count() + // Label bytes per line
             static_cast<size_t>(result->get_n2()) *  // Logical samples
                 result->get_n3() *                   // Logical bands
                 result->get_pixel_byte_count());     // Bytes per pixel

        // Append any EOL labels
        if (comments.find("EOL=1") != std::string::npos)
        {
            ht_file.seekg(
                data_offset + static_cast<std::streamoff>(data_length),
                std::ios::beg);

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

            // Append the EOL labels to the comments and parse them again
            append_from_file(comments, ht_file, eol_label_size);
            result->set_labels(comments);
        }

        const bool raw_data_is_int = result->format == BYTE ||
            result->format == HALF || result->format == FULL;

        const bool perform_swap =
            (raw_data_is_int && result->int_opposite_endian) ||
            (!raw_data_is_int && result->real_opposite_endian);

        const Decoder decode = decoder_for(result->format, perform_swap);
        if (decode == nullptr)
        {
            throw std::runtime_error(path + ": Unhandled format type" +
                                     std::to_string(result->format));
        }

        // Read the pixel data
        ht_file.seekg(data_offset, std::ios::beg);

        std::vector<uint8_t> data_array(data_length);
        ht_file.read(reinterpret_cast<char *>(data_array.data()),
                     static_cast<std::streamsize>(data_length));

        // We're done with reading the file directly
        ht_file.close();

        // Process the pixel data in the data_array into the pixel array,
        // which is always BSQ. Each run of N1 raw pixels is contiguous in the
        // file; where it lands in the pixel array depends on the
        // organization:
        //
        // BSQ  n1 = sample     n2 = line       n3 = band
        // BIL  n1 = sample     n2 = band       n3 = line
        // BIP  n1 = band       n2 = sample     n3 = line
        const int fmt_size = result->get_pixel_byte_count();
        const size_t plane = static_cast<size_t>(result->NL) * result->NS;
        const size_t run_bytes = static_cast<size_t>(result->N1) * fmt_size;

        result->pixel_data = std::unique_ptr<double[]>(
            new double[plane * static_cast<size_t>(result->NB)]);

        for (int n3 = 0; n3 < result->N3; n3++)
        {
            const size_t n3_offset =
                static_cast<size_t>(n3) * result->NBB + // Prior lines'
                                                        // prefixes
                static_cast<size_t>(n3) * result->N2 * run_bytes + // Prior
                                                                   // lines'
                                                                   // data
                result->NBB; // Current line's prefix

            for (int n2 = 0; n2 < result->N2; n2++)
            {
                const size_t offset = n3_offset +
                    static_cast<size_t>(n2) * run_bytes; // This line's prior
                                                         // samples

                double *destination = result->pixel_data.get();
                size_t stride = 1;

                switch (result->org)
                {
                case BSQ:
                    destination += result->pixel_index(0, n2, n3);
                    break;
                case BIL:
                    destination += result->pixel_index(0, n3, n2);
                    break;
                case BIP:
                    destination += result->pixel_index(n2, n3, 0);
                    stride = plane;
                    break;
                }

                decode(&data_array[offset], destination, result->N1, stride);
            }
        }

        return result;
    }

    bool is_number(const std::string &n)
    {
        static const std::regex number_regex(R"([+-]?\d+(?:\.\d*)?)");
        return std::regex_match(n, number_regex);
    }

    namespace
    {
        // Whether set_labels would read `value` back as a parenthesized list:
        // it takes everything up to the first ')' as the list, so a value
        // with a ')' anywhere but at the end is not one, whatever it starts
        // with
        bool is_list(const std::string &value)
        {
            return value.size() >= 2 && value.front() == '(' &&
                value.back() == ')' &&
                value.find(')') == value.size() - 1;
        }

        // Write one label the way set_labels reads it back: numbers and
        // parenthesized lists bare, anything else quoted, with a quote inside
        // a string doubled up. A quoted string and a bare list are stored the
        // same way, so a string that merely starts with '(' has to be told
        // apart by whether it would survive being read back bare.
        void write_label(std::ostream &labels,
                         const std::string &tag,
                         const std::string &value)
        {
            labels << tag << '=';

            if (is_number(value) || is_list(value))
            {
                labels << value;
            }
            else
            {
                labels << '\'';
                for (const char c : value)
                {
                    if (c == '\'')
                    {
                        labels << '\'';
                    }
                    labels << c;
                }
                labels << '\'';
            }

            labels << ' ';
        }
    }

    void VicarData::write_vicarfile(const std::string &path) const
    {
        const Encoder encode = encoder_for(format);
        if (encode == nullptr)
        {
            throw std::runtime_error {"Unable to handle complex values!"};
        }

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
            write_label(labels, label.first, label.second);
        }

        for (const auto &group : label_values)
        {
            labels << "PROPERTY='" << group.first << "' ";
            for (const auto &label : group.second)
            {
                write_label(labels, label.first, label.second);
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

        // Encode a line at a time rather than a pixel at a time
        std::vector<uint8_t> line(static_cast<size_t>(rec_size));

        for (int b = 0; b < NB; ++b)
        {
            for (int y = 0; y < NL; ++y)
            {
                encode(&pixel_data[pixel_index(0, y, b)], line.data(), NS);
                outfile.write(reinterpret_cast<const char *>(line.data()),
                              static_cast<std::streamsize>(line.size()));
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

    TerrainBounds VicarData::get_map_bounds() const
    {
        TerrainBounds bounds;

        auto property = label_values.find("SURFACE_PROJECTION_PARMS");
        if (property == label_values.end())
        {
            return bounds;
        }

        const auto &labels = property->second;
        auto x_min_iter = labels.find("X_AXIS_MINIMUM");
        auto y_min_iter = labels.find("Y_AXIS_MINIMUM");
        auto map_scale_iter = labels.find("MAP_SCALE");

        if (x_min_iter == labels.end() || y_min_iter == labels.end() ||
            map_scale_iter == labels.end())
        {
            return bounds;
        }

        double x_min, y_min, map_scale_x, map_scale_y;

        try
        {
            x_min = std::stod(x_min_iter->second);
        }
        catch (const std::logic_error &)
        {
            return bounds;
        }

        try
        {
            y_min = std::stod(y_min_iter->second);
        }
        catch (const std::logic_error &)
        {
            return bounds;
        }

        const std::string &map_scale_str = map_scale_iter->second;
        if (map_scale_str.find('(') != std::string::npos)
        {
            if (sscanf(map_scale_str.c_str(),
                       "(%lf,%lf)",
                       &map_scale_x,
                       &map_scale_y) != 2)
            {
                return bounds;
            }
        }
        else
        {
            try
            {
                map_scale_x = map_scale_y = std::stod(map_scale_str);
            }
            catch (const std::logic_error &)
            {
                return bounds;
            }
        }

        if (get_width() < 1 || get_height() < 1)
        {
            return bounds;
        }

        // Bounds describe the extent of the pixel centers, which is what the
        // X_AXIS_MAXIMUM and Y_AXIS_MAXIMUM labels report: the last pixel is
        // at (width - 1, height - 1), not (width, height).
        bounds.valid = true;
        bounds.min_x = x_min;
        bounds.min_y = y_min;
        bounds.max_x = x_min + ((get_width() - 1) * map_scale_x);
        bounds.max_y = y_min + ((get_height() - 1) * map_scale_y);

        return bounds;
    }
} // namespace rsvp
