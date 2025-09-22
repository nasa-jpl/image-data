#include <composite_data.h>
#include <translated_data.h>
#include <vicar_data.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include <cmath>

#include "Config.h"

namespace
{

    // The fixture for testing terrains
    class CompositeTest : public ::testing::Test
    {
    protected:
        CompositeTest()
        {
            const std::string mod_root =
                std::string(IMG_DATA_TEST_SOURCE_DIR) + "/terrain";

            // Make the AlphaBlendingCompositeData with left first and right
            // second.
            {
                auto raw_left_frame = rsvp::VicarData::read_vicarfile(
                    mod_root +
                    "/wedge/NLB_530659343RASLF0582340NCAM00385M1.ht");
                auto raw_right_frame = rsvp::VicarData::read_vicarfile(
                    mod_root +
                    "/wedge/NLB_530659366RASLF0582340NCAM00385M1.ht");
                raw_left_frame->set_alpha_band(2);
                raw_right_frame->set_alpha_band(2);

                // These values are copied straight out of the penultimate .mod
                // file loaded above
                const auto left_translated =
                    std::make_shared<rsvp::TranslatedData>(raw_left_frame,
                                                           -245.473613,
                                                           73.218732,
                                                           0.055684,
                                                           0,
                                                           0,
                                                           0.055684);
                const auto right_translated =
                    std::make_shared<rsvp::TranslatedData>(raw_right_frame,
                                                           -261.630795,
                                                           66.364738,
                                                           0.074289,
                                                           0,
                                                           0,
                                                           0.074289);

                alpha_blending_lr = new rsvp::AlphaBlendingCompositeData();
                alpha_blending_lr->add_image(left_translated);
                alpha_blending_lr->add_image(right_translated);
            }

            // Make the AlphaBlendingCompositeData with right first and left
            // second.
            {
                const auto raw_left_frame = rsvp::VicarData::read_vicarfile(
                    mod_root +
                    "/wedge/NLB_530659343RASLF0582340NCAM00385M1.ht");
                const auto raw_right_frame = rsvp::VicarData::read_vicarfile(
                    mod_root +
                    "/wedge/NLB_530659366RASLF0582340NCAM00385M1.ht");
                raw_left_frame->set_alpha_band(2);
                raw_right_frame->set_alpha_band(2);

                // This is not actually a memory leak - both of the
                // alpha_blending objects will clean up their own data when
                // they are destroyed. The only reason to have these defined as
                // class variables is to let us access the data directly to
                // examine how the blending works. No need to delete these
                // objects!
                const auto left_translated =
                    std::make_shared<rsvp::TranslatedData>(raw_left_frame,
                                                           -245.473613,
                                                           73.218732,
                                                           0.055684,
                                                           0,
                                                           0,
                                                           0.055684);
                const auto right_translated =
                    std::make_shared<rsvp::TranslatedData>(raw_right_frame,
                                                           -261.630795,
                                                           66.364738,
                                                           0.074289,
                                                           0,
                                                           0,
                                                           0.074289);

                alpha_blending_rl = new rsvp::AlphaBlendingCompositeData();
                alpha_blending_rl->add_image(right_translated);
                alpha_blending_rl->add_image(left_translated);
            }

            // Make the AverageCompositeData in whatever order
            const auto raw_left_frame = rsvp::VicarData::read_vicarfile(
                mod_root + "/wedge/NLB_530659343RASLF0582340NCAM00385M1.ht");
            const auto raw_right_frame = rsvp::VicarData::read_vicarfile(
                mod_root + "/wedge/NLB_530659366RASLF0582340NCAM00385M1.ht");

            left_frame.reset(new rsvp::TranslatedData(raw_left_frame,
                                                      -245.473613,
                                                      73.218732,
                                                      0.055684,
                                                      0,
                                                      0,
                                                      0.055684));
            right_frame.reset(new rsvp::TranslatedData(raw_right_frame,
                                                       -261.630795,
                                                       66.364738,
                                                       0.074289,
                                                       0,
                                                       0,
                                                       0.074289));

            average_composite = new rsvp::AverageCompositeData();
            average_composite->add_image(left_frame);
            average_composite->add_image(right_frame);
        }

        virtual ~CompositeTest()
        {
            delete average_composite;
            delete alpha_blending_lr;
            delete alpha_blending_rl;
        }

        virtual void SetUp()
        {
        }

        rsvp::AverageCompositeData *average_composite;

        rsvp::AlphaBlendingCompositeData *alpha_blending_lr;
        rsvp::AlphaBlendingCompositeData *alpha_blending_rl;

        std::shared_ptr<rsvp::ImageData> left_frame;
        std::shared_ptr<rsvp::ImageData> right_frame;
    };

    TEST_F(CompositeTest, data_compositing)
    {

        // This is a point that is defined in both the left and right
        // penultimate wedges
        double x = -230.23;
        double y = 79.467;

        double left_value = 0.0;
        double right_value = 0.0;
        int left_alpha = 0;
        int right_alpha = 0;

        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_double(left_value, x, y, 1));
        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_int(left_alpha, x, y, 2));

        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_double(right_value, x, y, 1));
        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_int(right_alpha, x, y, 2));

        // Confirm that both values are what we expect, and are not the same
        EXPECT_DOUBLE_EQ(left_value, -18.575354120349648);
        EXPECT_DOUBLE_EQ(right_value, -18.567674193641);

        // Confirm that both alpha values are 255
        EXPECT_EQ(left_alpha, 255);
        EXPECT_EQ(right_alpha, 255);

        double average_composite_value = 0.0;
        double alpha_blending_lr_value = 0.0;
        double alpha_blending_rl_value = 0.0;

        EXPECT_TRUE(average_composite->get_interpolated_pixel_double(
            average_composite_value, x, y, 0));
        EXPECT_TRUE(alpha_blending_lr->get_interpolated_pixel_double(
            alpha_blending_lr_value, x, y, 0));
        EXPECT_TRUE(alpha_blending_rl->get_interpolated_pixel_double(
            alpha_blending_rl_value, x, y, 0));

        // The AverageCompositeData built by ModData::read_modfile does a
        // strict weighted average; as both the left and right wedges have an
        // equal (255) alpha value at this point, the penultimate should return
        // their strict average.
        EXPECT_DOUBLE_EQ(average_composite_value,
                         ((left_value + right_value) / 2.0));

        // As both wedges have non-interpolated data and we added the right
        // wedge last, confirm that the value returned from the lr composite is
        // the same as just the right wedge
        EXPECT_DOUBLE_EQ(alpha_blending_lr_value, right_value);

        // As both wedges have non-interpolated data and we added the right
        // wedge last, confirm that the value returned from the rl composite is
        // the same as just the right wedge
        EXPECT_DOUBLE_EQ(alpha_blending_rl_value, left_value);

        // This is a point that is only defined in the left wedge
        x = -227.23;
        y = 79.294;

        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_double(left_value, x, y, 1));
        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_double(right_value, x, y, 1));

        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_int(left_alpha, x, y, 2));
        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_int(right_alpha, x, y, 2));

        // Confirm that both values are what we expect, and are not the same
        EXPECT_DOUBLE_EQ(left_value, -18.247353357419474);
        EXPECT_DOUBLE_EQ(right_value, -18.507902645097619);

        // Confirm that only the left channel has data (alpha=1 corresponds to
        // 0)
        EXPECT_EQ(left_alpha, 255);
        EXPECT_EQ(right_alpha, 1);

        EXPECT_TRUE(average_composite->get_interpolated_pixel_double(
            average_composite_value, x, y, 1));
        EXPECT_TRUE(alpha_blending_lr->get_interpolated_pixel_double(
            alpha_blending_lr_value, x, y, 1));
        EXPECT_TRUE(alpha_blending_rl->get_interpolated_pixel_double(
            alpha_blending_rl_value, x, y, 1));

        // As only the left wedge has data, confirm that the composite is the
        // same as just the left wedge
        EXPECT_DOUBLE_EQ(average_composite_value, left_value);
        EXPECT_DOUBLE_EQ(alpha_blending_lr_value, left_value);
        EXPECT_DOUBLE_EQ(alpha_blending_rl_value, left_value);

        // This is a point that is only defined in the right wedge
        x = -231.23;
        y = 76.688;

        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_double(left_value, x, y, 1));
        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_double(right_value, x, y, 1));

        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_int(left_alpha, x, y, 2));
        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_int(right_alpha, x, y, 2));

        // Confirm that both values are what we expect, and are not the same
        EXPECT_DOUBLE_EQ(left_value, -18.549896045487039);
        EXPECT_DOUBLE_EQ(right_value, -18.63500712720295);

        // Confirm that only the right channel has data (alpha=1 corresponds to
        // 0)
        EXPECT_EQ(left_alpha, 1);
        EXPECT_EQ(right_alpha, 255);

        EXPECT_TRUE(average_composite->get_interpolated_pixel_double(
            average_composite_value, x, y, 0));
        EXPECT_TRUE(alpha_blending_lr->get_interpolated_pixel_double(
            alpha_blending_lr_value, x, y, 0));
        EXPECT_TRUE(alpha_blending_rl->get_interpolated_pixel_double(
            alpha_blending_rl_value, x, y, 0));

        // As only the right wedge has data, confirm that the composite is the
        // same as just the right wedge
        EXPECT_DOUBLE_EQ(average_composite_value, right_value);
        EXPECT_DOUBLE_EQ(alpha_blending_lr_value, right_value);
        EXPECT_DOUBLE_EQ(alpha_blending_rl_value, right_value);


        // This is a point that is only defined in the left wedge, but is
        // _very_ close to the right wedge
        x = -228.98;
        y = 78.815;

        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_double(left_value, x, y, 1));
        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_double(right_value, x, y, 1));

        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_int(left_alpha, x, y, 2));
        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_int(right_alpha, x, y, 2));

        // Confirm that both values are what we expect, and are not the same
        EXPECT_DOUBLE_EQ(left_value, -18.465764434323383);
        EXPECT_DOUBLE_EQ(right_value, -18.491406141641164);

        // Confirm that the left wedge is fully defined, but the right wedge
        // has a good guess
        EXPECT_EQ(left_alpha, 255);
        EXPECT_EQ(right_alpha, 220);

        EXPECT_TRUE(average_composite->get_interpolated_pixel_double(
            average_composite_value, x, y, 1));
        EXPECT_TRUE(alpha_blending_lr->get_interpolated_pixel_double(
            alpha_blending_lr_value, x, y, 1));
        EXPECT_TRUE(alpha_blending_rl->get_interpolated_pixel_double(
            alpha_blending_rl_value, x, y, 1));

        // Compute the weighted average of the wedges
        double interpolated_value =
            (left_value * left_alpha + right_value * right_alpha) /
            (left_alpha + right_alpha);

        // The AverageCompositeData result should match the weighted average.
        EXPECT_DOUBLE_EQ(average_composite_value, interpolated_value);

        // The lr AlphaBlendingCompositeData should not match the left-wedge,
        // because the right-wedge comes later and has a non-minimum alpha
        // value
        EXPECT_NE(alpha_blending_lr_value, interpolated_value);
        EXPECT_GT(left_value, alpha_blending_lr_value);
        EXPECT_GT(alpha_blending_lr_value, right_value);

        // The alpha blended value should be closer to the left-wedge value
        // than the weighted average because we heaviliy weight real data over
        // extrapolated data in the alpha-blending algorithm.
        double wavg_dist_to_right_wedge =
            std::abs(right_value - interpolated_value);
        double alpha_dist_to_right_wedge =
            std::abs(right_value - alpha_blending_lr_value);
        EXPECT_GT(alpha_dist_to_right_wedge, wavg_dist_to_right_wedge);

        // For the rl version of the AverageCompositeData, the result should
        // exactly match the left wedge, as it comes last
        EXPECT_DOUBLE_EQ(alpha_blending_rl_value, left_value);


        // This is a point that is not defined in either wedge, but is very
        // close to both
        x = -227.62;
        y = 76.599;

        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_double(left_value, x, y, 1));
        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_double(right_value, x, y, 1));

        EXPECT_TRUE(
            left_frame->get_interpolated_pixel_int(left_alpha, x, y, 2));
        EXPECT_TRUE(
            right_frame->get_interpolated_pixel_int(right_alpha, x, y, 2));

        // Confirm that they are not the same
        EXPECT_NE(left_value, right_value);

        // Confirm that the left wedge has a better guess than the right wedge
        EXPECT_EQ(left_alpha, 197);
        EXPECT_EQ(right_alpha, 164);

        EXPECT_TRUE(average_composite->get_interpolated_pixel_double(
            average_composite_value, x, y, 1));
        EXPECT_TRUE(alpha_blending_lr->get_interpolated_pixel_double(
            alpha_blending_lr_value, x, y, 1));
        EXPECT_TRUE(alpha_blending_rl->get_interpolated_pixel_double(
            alpha_blending_rl_value, x, y, 1));

        // Compute the weighted average of the wedges
        interpolated_value =
            (left_value * left_alpha + right_value * right_alpha) /
            (left_alpha + right_alpha);

        // The AverageCompositeData result should match the weighted average.
        EXPECT_DOUBLE_EQ(average_composite_value, interpolated_value);

        // Both of the lr and the rl AlphaBlendingCompositeData should be
        // between the left and right values, but the lr version should be
        // closer to the right wedge and the rl version should be closer to the
        // left wedge.
        if (left_value > right_value)
        {
            EXPECT_GT(left_value, alpha_blending_rl_value);
            EXPECT_GT(alpha_blending_rl_value, alpha_blending_lr_value);
            EXPECT_GT(alpha_blending_lr_value, right_value);
        }
        else
        {
            EXPECT_LT(left_value, alpha_blending_rl_value);
            EXPECT_LT(alpha_blending_rl_value, alpha_blending_lr_value);
            EXPECT_LT(alpha_blending_lr_value, right_value);
        }
    }
}
