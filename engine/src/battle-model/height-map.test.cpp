// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "./height-map.h"

BOOST_AUTO_TEST_SUITE(battlemodel_heightmap)

  BOOST_AUTO_TEST_CASE(default_map_should_have_zero_height)
  {
      HeightMap heightMap(bounds2f{0.0f, 0.0f, 10.0f, 10.0f});
      for (int x = -1; x != 10; ++x) {
          for (int y = -1; y != 10; ++y) {
              BOOST_CHECK_EQUAL(0.0f, heightMap.getHeight(x, y));
          }
      }
  }

  BOOST_AUTO_TEST_CASE(map_should_return_updated_height)
  {
      HeightMap heightMap(bounds2f{0.0f, 0.0f, 10.0f, 10.0f});
      heightMap.update({5, 5}, [](auto x, auto y) { return x * y; });
      for (int x = 0; x != 5; ++x) {
          for (int y = 0; y != 5; ++y) {
              BOOST_CHECK_EQUAL(x * y, heightMap.getHeight(x, y));
          }
      }
  }

BOOST_AUTO_TEST_SUITE_END()
