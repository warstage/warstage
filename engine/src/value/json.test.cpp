// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "./json.h"


BOOST_AUTO_TEST_SUITE(value_json)

  BOOST_AUTO_TEST_CASE(parse_document) {
    auto is = std::istringstream{R"({"foo":true, "bar":123,"hello" : "world"})"};
    Value value;
    is >> value;
    std::ostringstream os;
    os << value;
    BOOST_CHECK_EQUAL(os.str(), R"({"foo":true,"bar":123,"hello":"world"})");
  }

  BOOST_AUTO_TEST_CASE(parse_document_unit) {
    auto is = std::istringstream{R"JSON({
      "unitType": {
        "subunits": [{
          "element": {
            "size": [1.1, 2.5, 2.3],
            "shape": "GEN-KATA",
            "movement": {
              "speed": { "normal": 7.0, "fast": 14.0 }
            }
          },
          "individuals": 40,
          "weapons": [{
            "melee": {
              "reach": 1.0,
              "time": { "ready": 1.0, "strike": 1.8 }
            }
          }]
        }],
        "formations": [{
          "name": "Line",
          "type": "LINE",
          "spacing": [1.1, 1.7],
          "ranks": 5
        }],
        "training": 0.9
      },
      "shape": {
        "size": [1.0, 3.0, 3.0],
        "skin": "cav",
        "line": "kata"
      },
      "marker": {
        "texture": "markers.png",
        "texgrid": 16,
        "layers": [
          { "vertices": [[0, 0], [3, 3]], "state": { "hostile": true, "command": true, "dragged": false } },
          { "vertices": [[3, 0], [6, 3]], "state": { "allied": true, "command": true, "dragged": false } },
          { "vertices": [[6, 0], [9, 3]], "state": { "friendly": true, "command": true, "dragged": false } },
          { "vertices": [[9, 0], [12, 3]], "state": { "routed": true, "command": true, "dragged": false } },

          { "vertices": [[0, 3], [3, 6]], "state": { "hostile": true, "command": false, "dragged": false } },
          { "vertices": [[3, 3], [6, 6]], "state": { "allied": true, "command": false, "dragged": false } },
          { "vertices": [[6, 3], [9, 6]], "state": { "friendly": true, "command": false, "dragged": false } },
          { "vertices": [[9, 3], [12, 6]], "state": { "routed": true, "command": false, "dragged": false } },

          { "vertices": [[0, 6], [ 3, 9]], "state": { "hostile": true, "dragged": true } },
          { "vertices": [[3, 6], [ 6, 9]], "state": { "allied": true, "dragged": true } },
          { "vertices": [[6, 6], [ 9, 9]], "state": { "friendly": true, "dragged": true } },

          { "vertices": [[0, 9], [3, 12]], "state": { "dragged": false } },
          { "vertices": [[12, 3], [15, 6]], "state": { "dragged": false } },

          { "vertices": [[9, 3], [12, 6]], "state": { "selected": true, "dragged": false } },
          { "vertices": [[9, 3], [12, 6]], "state": { "selected": true, "dragged": false } },
          { "vertices": [[9, 3], [12, 6]], "state": { "hovered": true, "dragged": false } }
        ]
      }
    })JSON"};
    Value value;
    is >> value;
  }

  BOOST_AUTO_TEST_CASE(empty_document) {
        std::ostringstream os;
        os << (Struct{} << ValueEnd{});
        BOOST_CHECK_EQUAL(os.str(), "{}");
    }

    BOOST_AUTO_TEST_CASE(simple_document1) {
        std::ostringstream os;
        os << (Struct{}
                << "foo" << true
                << "bar" << 123
                << "hello" << "world"
                << ValueEnd{});
        BOOST_CHECK_EQUAL(os.str(), R"({"foo":true,"bar":123,"hello":"world"})");
    }

    BOOST_AUTO_TEST_CASE(simple_document2) {
        std::ostringstream os;
        os << (Struct{}
            << "foo" << false
            << "bar" << 123.456
            << "hello" << nullptr
            << ValueEnd{});
        BOOST_CHECK_EQUAL(os.str(), R"({"foo":false,"bar":123.456,"hello":null})");
    }

    BOOST_AUTO_TEST_CASE(simple_array) {
        std::ostringstream os;
        os << (Array{}
                << nullptr
                << false
                << true
                << 123
                << 4.56
                << "hello world"
                << ValueEnd{});
        BOOST_CHECK_EQUAL(os.str(), R"([null,false,true,123,4.56,"hello world"])");
    }

    BOOST_AUTO_TEST_CASE(escaped_string) {
        std::ostringstream os;
        os << (Array{} << "\\/\b\f\n\r\t" << ValueEnd{});
        BOOST_CHECK_EQUAL(os.str(), "[\"\\\\\\/\\b\\f\\n\\r\\t\"]");
    }

BOOST_AUTO_TEST_SUITE_END()
