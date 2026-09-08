#include "mod_data.h"

#include "composite_data.h"
#include "translated_data.h"
#include "vicar_data.h"
#include "z_offset_data.h"

#include <stdexcept>

#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <utility>


namespace rsvp
{

    // Parse the `alpha_band` from the properties tokens
    static bool apply_properties(const std::shared_ptr<ImageData> &image,
                                 std::list<std::string> *tokens,
                                 const std::string &filename)
    {
        // No image is a failure case
        if (!image)
        {
            return false;
        }

        // No tokens is not a failure case - there are just no properties!
        if (tokens->empty())
        {
            return true;
        }

        if (tokens->size() < 3 || tokens->front() != "[" ||
            tokens->back() != "]")
        {
            std::string prop_string;
            for (auto &t : *tokens)
            {
                prop_string += t;
            }
            throw std::runtime_error(
                filename +
                ": Invalid property arguments in terrain mod file: " +
                prop_string);
        }

        // Pop off the opening and closing brackets
        tokens->pop_front();
        tokens->pop_back();

        while (not tokens->empty())
        {
            if (tokens->front() == "alpha_band")
            {
                if (tokens->size() < 2)
                {
                    // `alpha_band` needs a channel
                    throw std::runtime_error(
                        filename +
                        ": Invalid syntax in terrain mod file: "
                        "`alpha_band` requires a band number");
                }

                // Pop off the `alpha_band`
                tokens->pop_front();

                int alpha_band;
                try
                {
                    size_t len;
                    alpha_band = std::stoi(tokens->front(), &len, 10);

                    // Check that we consumed the entire token - otherwise
                    // something went wrong and we should abort
                    if (len < tokens->front().size())
                    {
                        throw std::logic_error("");
                    }
                }
                catch (const std::logic_error &)
                {
                    throw std::runtime_error(
                        filename +
                        ": Invalid syntax in terrain mod file: "
                        "error parsing alpha_band; invalid int value `" +
                        tokens->front() + "`");
                }

                // Pop off the value
                tokens->pop_front();
                // Apply it to the image
                image->set_alpha_band(alpha_band);
            }
            else
            {
                throw std::runtime_error(
                    filename +
                    ": Found invalid image property token in terrain mod "
                    "file: " +
                    tokens->front());
            }
        }

        return true;
    }

    std::shared_ptr<ImageData>
    ModData::read_modfile(const std::string &filename)
    {
        std::ifstream mod_file(filename);

        std::string token;
        std::list<std::string> tokens;

        // If we failed to open the file, abort
        if (!mod_file.is_open())
        {
            throw std::runtime_error("Could not open mod file: " + filename);
        }

        // Build up a list of whitespace-separated tokens
        while (mod_file >> token)
        {
            tokens.push_back(token);
        }

        mod_file.close();

        // If the first token is not "I~", this is not a valid .mod file
        if (tokens.empty() or tokens.front() != "I~")
        {
            throw std::runtime_error(
                filename + ": No leading `I~` in this mod file - saw `" +
                token + "` instead!");
        }

        // Remove the leading "I~"
        tokens.pop_front();

        // As far as we can tell, we've got a valid mod file - so return the
        // parsing of that!
        return parse_imagedata(&tokens, filename);
    }

    std::shared_ptr<ImageData>
    ModData::read_bare_vicarfile(const std::string &filename)
    {
        std::shared_ptr<VicarData> raw_result =
            VicarData::read_vicarfile(filename);

        if (raw_result)
        {
            const size_t extension_pos = filename.find_last_of('.');
            const std::string extension =
                (extension_pos == std::string::npos) ?
                "" :
                filename.substr(extension_pos + 1, std::string::npos);

            // Try to set the alpha channel appropriately for terrain
            // classification ".tc" files and heightmap ".ht" files.
            const auto alpha_band_to_set =
                (extension == "tc" or extension == "TC") ? 1 : 2;

            raw_result->set_alpha_band(alpha_band_to_set);

            double x_offset = 0.0;
            double y_offset = 0.0;
            double map_scale = 1.0;

            std::string token;
            size_t equal_pos;
            std::stringstream comment_stream(raw_result->get_labels());

            while (comment_stream >> token)
            {
                equal_pos = token.find('=');

                // We're tokenizing on spaces, so it's always possible to break
                // up a quoted string by accident - just ignore those for our
                // use case
                if (equal_pos == std::string::npos)
                {
                    continue;
                }

                std::string tag = token.substr(0, equal_pos);
                std::string value =
                    token.substr(equal_pos + 1, std::string::npos);

                if (tag == "TASK")
                {
                    // On MSL, the labels we care about appear in the system
                    // labels (before any PROPERTY labels). On M2020, they
                    // appear in the PROPERTY=SURFACE_PROJECTION_PARMS section.
                    // So... in either case, if we hit a TASK, we've gone too
                    // far, but otherwise we might be vulnerable to repeated
                    // keys.
                    break;
                }

                if (tag == "X_AXIS_MINIMUM")
                {
                    std::stringstream valuestream(value);
                    valuestream >> x_offset;
                }
                else if (tag == "Y_AXIS_MINIMUM")
                {
                    std::stringstream valuestream(value);
                    valuestream >> y_offset;
                }
                else if (tag == "MAP_SCALE")
                {
                    if (value.find('(') != std::string::npos)
                    {
                        // We have an M2020-style
                        // `MAP_SCALE=(0.0158105,0.0158105)`.
                        double other_scale = -1.0;
                        if (sscanf(value.c_str(),
                                   "(%lf,%lf)",
                                   &map_scale,
                                   &other_scale) != 2)
                        {
                            throw std::runtime_error(
                                filename +
                                ": Could not parse a MAP_SCALE from `" +
                                value + "`");
                        }
                    }
                    else
                    {
                        // We have an MSL-style `MAP_SCALE=0.046456`
                        std::stringstream valuestream(value);
                        valuestream >> map_scale;
                    }
                }
            }

            auto result = std::make_shared<AlphaBlendingCompositeData>();

            // .mod files do not support rotations, so that parameter will
            // always be zero.
            result->add_image(std::make_shared<TranslatedData>(
                raw_result, x_offset, y_offset, map_scale, 0.0));

            result->set_alpha_band(alpha_band_to_set);
            return result;
        }

        return nullptr;
    }

    std::shared_ptr<ImageData>
    ModData::parse_imagedata(std::list<std::string> *tokens,
                             const std::string &filename)
    {
        /* Per the VSL README.MOD file:

            Following this header, which is used by the Image class to
            identify the mod file type, is a block which specifies some
            combination of one or more images with the following form:

            {  block_type   block_argument_1 ... block_argument_n
                [ optional_block_parameters ]
            }

            Note that all separators {,},[,],etc. must be surrounded
            by white space characters for the parser to correctly
            identify them.

        We therefore expect the next non-blank character to be a '{',
        and the last to be a '}'.
        */

        // Special handling of "empty" case
        if (tokens->size() == 2 && tokens->front() == "{" &&
            tokens->back() == "}")
        {
            throw std::runtime_error(
                filename +
                ": Invalid image arguments; argument is an empty block {}; "
                "expects at least one argument in the form: { block_type "
                "block_argument_1 ... block_argument_n "
                "[ optional_block_parameters ] }");
        }

        if (tokens->size() < 3 || tokens->front() != "{" ||
            tokens->back() != "}")
        {
            throw std::runtime_error(
                filename +
                ": Invalid image arguments: expects at least one argument in "
                "the form: { block_type block_argument_1 ... "
                "block_argument_n [ optional_block_parameters ] }");
        }
        tokens->pop_front();
        tokens->pop_back();

        if (tokens->front() == "composite" || tokens->front() == "scoredmap")
        {
            return parse_compdata(tokens, filename);
        }
        else if (tokens->front() == "transform")
        {
            return parse_transdata(tokens, filename);
        }
        else if (tokens->front() == "file")
        {
            return parse_filedata(tokens, filename);
        }
        else if (tokens->front() == "deinterpolate")
        {
            return parse_deinterpolate(tokens, filename);
        }
        else if (tokens->front() == "zoffset")
        {
            return parse_zoffset(tokens, filename);
        }
        else
        {
            throw std::runtime_error(
                filename + ": Unknown Image arguments: " + tokens->front());
        }
    }

    std::shared_ptr<ImageData>
    ModData::parse_zoffset(std::list<std::string> *tokens,
                           const std::string &filename)
    {
        // zoffset blocks have the form:
        //
        //     { zoffset block z_offset z_scale }
        //
        //  ModData::parse_imagedata stripped off the surrounding brackets

        if (tokens->size() < 4 || tokens->front() != "zoffset")
        {
            std::string argument_string;
            for (auto &t : *tokens)
            {
                argument_string += t;
            }
            throw std::runtime_error(
                filename +
                ": Invalid z offset argument form: expected form `{ "
                "zoffset block z_offset z_scale }` but got `" +
                argument_string + "`");
        }

        tokens->pop_front();

        // Check that we appear to have a block
        if (tokens->front() != "{")
        {
            throw std::runtime_error(
                filename +
                ": Invalid z offset argument form: expected "
                "block as first argument");
        }

        // We now need to extract the embedded blocks
        // We know that the next token is a "{" - so we build up a new list of
        // tokens until we find the _matching_ (not just the next) "}"
        // character, and then send it through another round of parsing
        std::list<std::string> local_tokens;

        local_tokens.push_back(tokens->front());
        tokens->pop_front();

        int stack_depth = 1;

        for (;;)
        {
            local_tokens.push_back(tokens->front());

            if (tokens->front() == "{")
            {
                stack_depth += 1;
            }
            else if (tokens->front() == "}")
            {
                stack_depth -= 1;
            }

            tokens->pop_front();

            if (stack_depth <= 0)
            {
                break;
            }

            if (tokens->empty())
            {
                // We haven't found our final closing `}`, but we've run
                // out of tokens. Abort!
                throw std::runtime_error(
                    filename + ": Z offset arguments missing closing `}`");
            }
        }

        // We've extracted the embedded block, and now we need to extract the
        // offset and scale
        if (tokens->size() != 2)
        {
            throw std::runtime_error(
                filename +
                ": ZOffset block does not have proper offset and scale");
        }

        double z_offset;
        double z_scale;

        try
        {
            z_offset = std::stod(tokens->front());
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(
                filename + ": Error parsing z_offset: Invalid double value `" +
                tokens->front() + "`: " + e.what());
        }

        // Pop off the offset value
        tokens->pop_front();

        try
        {
            z_scale = std::stod(tokens->front());
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(
                filename + ": Error parsing z_scale: Invalid double value `" +
                tokens->front() + "`: " + e.what());
        }

        // Pop off the value
        tokens->pop_front();

        // We've now extracted everything - actually allocate some memory now.
        std::shared_ptr<ImageData> embedded =
            ModData::parse_imagedata(&local_tokens, filename);

        if (embedded == nullptr)
        {
            throw std::runtime_error(filename +
                                     ": Unable to parse embedded image data");
        }

        auto result = std::make_shared<ZOffsetData>(embedded);

        for (int band = 0; band < result->get_bands(); band++)
        {
            result->set_offset_and_scale(band, z_offset, z_scale);
        }

        return result;
    }

    std::shared_ptr<ImageData>
    ModData::parse_deinterpolate(std::list<std::string> *tokens,
                                 const std::string &filename)
    {
        // Deinterpolate blocks have the form:
        //
        //     { deinterpolate block }
        //
        //  ModData::parse_imagedata stripped off the surrounding brackets

        if (tokens->size() < 4 || tokens->front() != "deinterpolate")
        {
            std::string argument_string;
            for (auto &t : *tokens)
            {
                argument_string += t;
            }
            throw std::runtime_error(
                filename +
                ": Invalid deinterpolate arguments: arguments should have the "
                "form { deinterpolate block } but got " +
                argument_string);
        }

        tokens->pop_front();

        std::shared_ptr<ImageData> result = parse_imagedata(tokens, filename);

        if (result)
        {
            result->set_interpolating(false);
        }

        return result;
    }

    std::shared_ptr<ImageData>
    ModData::parse_filedata(std::list<std::string> *tokens,
                            const std::string &filename)
    {
        // Files have the form:
        //
        //     { file filespec [ parameters ] }
        //
        // ModData::parse_imagedata stripped off the surrounding brackets

        if (tokens->size() < 2 || tokens->front() != "file")
        {
            std::string argument_string;
            for (auto &t : *tokens)
            {
                argument_string += t;
            }
            throw std::runtime_error(
                filename +
                ": Invalid file arguments: arguments should have the "
                "form { file filespec [ parameters ] } but got " +
                argument_string);
        }

        // Pop off "file"
        tokens->pop_front();

        // Now we have the file path at the front of the list
        std::string path;
        if (tokens->front().at(0) == '/')
        {
            path = tokens->front();
        }
        else
        {
            size_t last_slash = filename.rfind('/');

            if (last_slash == std::string::npos)
            {
                throw std::runtime_error("Invalid mod data filepath. Expected "
                                         "absolute filepath. Got: " +
                                         filename);
            }

            path = filename.substr(0, last_slash) + "/" + tokens->front();
        }

        // Pop off the path
        tokens->pop_front();

        // We come to an interesting case. As currently established, .ht and
        // .tc files contain their own offsets/scalings, and we can create the
        // appropriate hierarchy of `TranslatedData` and `VicarData` objects
        // with `ModData::read_bare_vicarfile`. That way we can use a raw vicar
        // file as a heightmap in RSVP by specifying the following (pseudo)
        // RML:
        //
        //   <Terrain><Heightfile>heightmap.ht</Heightfile</Terrain>
        //
        // _However_, as currently used on MSL, mod files generated by OPGS
        // explicitly contain the the transforms extracted from the file
        // headers:
        //
        //   ~I { transform { file heightmap.ht } <translated_parms> }
        //
        // That means that we _must_ ignore the transform in the file headers
        // and must _not_ use `ModData::read_bare_vicarfile` to parse .ht and
        // .tc files to prevent double-transforming the data.

        std::string extension;

        size_t extension_pos = path.find_last_of('.');
        if (extension_pos != std::string::npos)
        {
            extension = path.substr(extension_pos + 1, std::string::npos);
        }

        std::shared_ptr<ImageData> result;

        if (extension == "ht" || extension == "HT" || extension == "tc" ||
            extension == "TC")
        {
            // Parse .ht and .tc files as raw, untransformed vicar files
            result = VicarData::read_vicarfile(path);
        }
        else
        {
            // Parse everything else in our usual manner
            result = ImageData::read(path);
        }

        // Fail fast if we didn't parse the file appropriately
        if (result == nullptr)
        {
            throw std::runtime_error(
                filename + ": Unable to read filedata from: " + path);
        }

        // Try to set the alpha channel appropriately for terrain
        // classification ".tc" files and heightmap ".ht" files.
        if (extension == "tc" || extension == "TC")
        {
            result->set_alpha_band(1);
        }
        else if (extension == "ht" || extension == "HT")
        {
            result->set_alpha_band(2);
        }

        // Now that we've set that default value, go apply the remaining
        // properties (which may overwrite the alpha_band)
        if (!apply_properties(result, tokens, filename))
        {
            throw std::runtime_error(
                filename + ": Unable to apply properties to image data");
        }

        return result;
    }

    std::shared_ptr<ImageData>
    ModData::parse_compdata(std::list<std::string> *tokens,
                            const std::string &filename)
    {
        // Composites have the form:
        //
        //     { composite block_1 block_2 ... block_n [ parameters ] }
        //
        // ModData::parse_imagedata stripped off the surrounding brackets.
        // This function also handles "scoredmap"s

        // Check for special "empty case". Added when addressing Issue #3071.
        bool is_valid_empty_case =
            (tokens->size() == 1 && tokens->front() == "composite");

        // We need the empty case or at least 3 tokens (`composite { }`)
        if (!is_valid_empty_case &&
            (tokens->size() < 3 ||
             (tokens->front() != "composite" &&
              tokens->front() != "scoredmap")))
        {
            std::string arg_string;
            for (auto &t : *tokens)
            {
                arg_string += t;
            }
            throw std::runtime_error(
                filename +
                ": Invalid composite arguments: expected arguments of the "
                "form `composite|scoredmap "
                "block_1 block_2 ... block_n [ parameters ]` but got: " +
                arg_string);
        }

        std::string composite_type = tokens->front();

        // Pop off "composite"
        tokens->pop_front();

        if (!is_valid_empty_case && tokens->front() != "{")
        {
            throw std::runtime_error(
                filename +
                ": Invalid composite arguments: first block missing `{`");
        }

        // We now need to extract the embedded blocks
        // We know that the next token is a "{" - so we build up a new list of
        // tokens until we find the _matching_ (not just the next) "}"
        // character, and then send it through another round of parsing
        std::shared_ptr<CompositeData> result;
        if (composite_type == "composite")
        {
            result = std::make_shared<AlphaBlendingCompositeData>();
        }
        else
        {
            result = std::make_shared<ScoredCompositeData>();
        }

        // Bail out quickly if we failed.
        if (result == nullptr)
        {
            throw std::runtime_error(
                filename + ": Unable to create the image's CompositeData");
        }

        while (not tokens->empty() and tokens->front() == "{")
        {
            std::list<std::string> local_tokens;

            local_tokens.push_back(tokens->front());
            tokens->pop_front();

            int stack_depth = 1;

            for (;;)
            {
                local_tokens.push_back(tokens->front());

                if (tokens->front() == "{")
                {
                    stack_depth += 1;
                }
                else if (tokens->front() == "}")
                {
                    stack_depth -= 1;
                }

                tokens->pop_front();

                if (stack_depth <= 0)
                {
                    break;
                }

                if (tokens->empty())
                {
                    // We haven't found our final closing `}`, but we've run
                    // out of tokens. Abort!
                    throw std::runtime_error(
                        filename +
                        ": Invalid composite arguments: contains "
                        "block missing closing `}`");
                }
            }

            std::shared_ptr<ImageData> embedded =
                ModData::parse_imagedata(&local_tokens, filename);

            if (embedded == nullptr)
            {
                throw std::runtime_error(
                    filename +
                    ": Invalid composite arguments: unable to parse "
                    "embedded image");
            }

            result->add_image(embedded);
        }

        // Try to set the alpha band appropriately for composited heightmaps
        // (.mod files) and composited terrain classifications (.mod_tc files)
        size_t extension_pos = filename.find_last_of('.');
        if (extension_pos != std::string::npos)
        {
            std::string file_extension =
                filename.substr(extension_pos + 1, std::string::npos);

            if (file_extension == "mod" || file_extension == "MOD")
            {
                result->set_alpha_band(2);
            }
            else if (file_extension == "mod_tc" || file_extension == "MOD_TC")
            {
                result->set_alpha_band(1);
            }

            // Now that we've set that default value, go apply the remaining
            // properties (which may overwrite the alpha_band)
            if (!apply_properties(result, tokens, filename))
            {
                throw std::runtime_error(
                    filename + ": Unable to apply properties to compdata");
            }
        }

        if (!tokens->empty())
        {
            std::string arg_string;
            for (auto &t : *tokens)
            {
                arg_string += t;
            }
            throw std::runtime_error(
                filename +
                ": Invalid composite arguments: found unexpected tokens: " +
                arg_string);
        }

        return result;
    }

    std::shared_ptr<ImageData>
    ModData::parse_transdata(std::list<std::string> *tokens,
                             const std::string &filename)
    {
        // Transforms have the form:
        //
        //     { transform block t1 t2 t3 t4 t5 t6 }
        //
        // ModData::parse_imagedata stripped off the surrounding brackets, so
        // we need at _least_ 8 tokens

        if (tokens->size() < 8 || tokens->front() != "transform")
        {
            std::string arg_string;
            for (auto &t : *tokens)
            {
                arg_string += t;
            }
            throw std::runtime_error(
                filename +
                ": Invalid transdata arguments: expected arguments of the "
                "form `transform block t1 t2 t3 t4 t5 t6` but got: " +
                arg_string);
        }

        // Pop off "transform"
        tokens->pop_front();

        double affine_transform[6] = {0};
        for (int i = 5; i >= 0; i--)
        {
            try
            {
                affine_transform[i] = std::stod(tokens->back());
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error(
                    filename +
                    ": Error parsing transform: invalid double value `" +
                    tokens->back() + "`: " + e.what());
            }

            tokens->pop_back();
        }

        // At this point, we've stripped off everything but the contained
        // block. For that... we go back to the start and recurse again!
        std::shared_ptr<ImageData> contents =
            ModData::parse_imagedata(tokens, filename);

        // If something failed in the deeper parsing, abort
        if (contents == nullptr)
        {
            throw std::runtime_error(filename +
                                     ": Unable to parse transform ImageData");
        }

        return std::make_shared<TranslatedData>(contents,
                                                affine_transform[0],
                                                affine_transform[1],
                                                affine_transform[2],
                                                affine_transform[3],
                                                affine_transform[4],
                                                affine_transform[5]);
    }
}
