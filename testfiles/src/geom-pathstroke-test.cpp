// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Test the geom-pathstroke functionality.
 */
/*
 * Authors:
 *   Hendrik Roehm <git@roehm.ws>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include "helper/geom-pathstroke.h"

#include <2geom/rect.h>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>

#include "document.h"
#include "inkscape.h"
#include "object/sp-item.h"
#include "object/sp-path.h"
#include "object/sp-rect.h"
#include "pathvector.h"
#include "svg/svg.h"
#include "util/units.h"

class InkscapeInit // Initialize the Inkscape Application singleton.
{
public:
    InkscapeInit()
    {
        if (!Inkscape::Application::exists()) {
            Inkscape::Application::create(false);
        }
    }
};

/**
 * @brief svg file based test fixture
 *
 * the svg file is expected to contain a text node with id num_tests. The content should be contain only the number of
 * test objects in the file. For each test objects, there should be two objects called "test-1" and "comp-1" where 1
 * ranges from 1 to test count as above.
 */
class GeomPathstrokeTest : public ::testing::Test
{
protected:
    GeomPathstrokeTest()
        : _init{}
        , _document{SPDocument::createNewDoc(INKSCAPE_TESTS_DIR "/data/geom-pathstroke.svg", false)}
    {
        _document->ensureUpToDate();
        _findTestCount();
    }

public:
    SPPath *getItemById(char const *const id)
    {
        auto obj = _document->getObjectById(id);
        if (!obj) {
            return nullptr;
        }
        return cast<SPPath>(obj);
    }
    size_t testCount() const { return _test_count; }

private:
    void _findTestCount()
    {
        auto const item = _document->getObjectById("num_tests");
        if (!item) {
            std::cerr << "Could not get the element with id=\"num_tests\".\n";
            return;
        }
        auto const tspan = item->firstChild();
        if (!tspan) {
            std::cerr << "Could not get the first child of element with id=\"num_tests\".\n";
            return;
        }
        auto const content = tspan->firstChild();
        if (!content) {
            std::cerr << "Could not get the content of the first child of element with id=\"num_tests\".\n";
            return;
        }
        auto const repr = content->getRepr();
        if (!repr) {
            std::cerr << "Could not get the repr of the content of the first child of element with id=\"num_tests\".\n";
            return;
        }
        auto const text = repr->content();
        if (!text) {
            std::cerr << "Could not get the text content of the first child of element with id=\"num_tests\".\n";
            return;
        }
        try {
            _test_count = std::stoul(text);
        } catch (std::invalid_argument const &e) {
            std::cerr << "Could not parse an integer from content of first child of element with id=\"num_tests\".\n";
            return;
        }
    }

    InkscapeInit _init;
    std::unique_ptr<SPDocument> _document;
    size_t _test_count = 0;
};

double approximate_directed_hausdorff_distance(const Geom::Path *path1, const Geom::Path *path2)
{
    auto const time_range = path1->timeRange();
    double dist_max = 0.;
    for (size_t i = 0; i <= 25; i += 1) {
        auto const time = time_range.valueAt(static_cast<double>(i) / 25);
        auto const search_point = path1->pointAt(time);
        Geom::Coord dist;
        path2->nearestTime(search_point, &dist);
        if (dist > dist_max) {
            dist_max = dist;
        }
    }

    return dist_max;
}

TEST_F(GeomPathstrokeTest, BoundedHausdorffDistance)
{
    size_t const id_maxlen = 7 + 1;
    char test_id[id_maxlen], comp_id[id_maxlen];
    double const tolerance = 0.1;
    // same as 0.1 inch in the document (only works without viewBox and transformations)
    auto const offset_width = -9.6;

    // assure that the num_tests field was found and there is at least one test
    ASSERT_GT(testCount(), 0);

    for (size_t i = 1; i <= testCount(); i++) {
        snprintf(test_id, id_maxlen, "test-%lu", i);
        snprintf(comp_id, id_maxlen, "comp-%lu", i);
        std::cout << "checking " << test_id << std::endl;

        auto const *test_item = getItemById(test_id);
        auto const *comp_item = getItemById(comp_id);
        ASSERT_TRUE(test_item && comp_item);

        auto const test_curve = test_item->curve();
        auto const comp_curve = comp_item->curve();
        ASSERT_TRUE(test_curve && comp_curve);

        auto const &test_pathvector = test_curve->get_pathvector();
        auto const &comp_pathvector = comp_curve->get_pathvector();
        ASSERT_EQ(test_pathvector.size(), 1);
        ASSERT_EQ(comp_pathvector.size(), 1);

        auto const &test_path = test_pathvector.at(0);
        auto const &comp_path = comp_pathvector.at(0);

        auto const offset_path = Inkscape::half_outline(test_path, offset_width, 0, Inkscape::JOIN_EXTRAPOLATE, 0.);
        auto const error1 = approximate_directed_hausdorff_distance(&offset_path, &comp_path);
        auto const error2 = approximate_directed_hausdorff_distance(&comp_path, &offset_path);

        if (error1 > tolerance || error2 > tolerance) {
            auto const pv_actual = Geom::PathVector(offset_path);
            std::cout << "actual d " << sp_svg_write_path(pv_actual, true) << std::endl;
            auto const pv_expected = Geom::PathVector(comp_path);
            std::cout << "expected d " << sp_svg_write_path(pv_expected, true) << std::endl;
            std::cout << "note that transforms etc are not considered. Therefore they shoud have equal transforms."
                      << std::endl;
        }
        ASSERT_LE(error1, tolerance);
        ASSERT_LE(error2, tolerance);
    }
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :