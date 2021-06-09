// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./json.h"


namespace {

  bool isWhitespace(std::istream::int_type c) {
    switch (c) {
      case ' ':
      case '\n':
      case '\r':
      case '\t':
        return true;
      default:
        return false;
    }
  }

  void parseWhitespace(std::istream& is) {
    while (isWhitespace(is.peek())) {
      is.get();
    }
  }

  void parseChar(std::istream& is, std::istream::int_type c) {
    const auto v = is.get();
    if (v != c) {
      throw std::invalid_argument(std::string("expected ") + static_cast<char>(c)
        + ", got " + static_cast<char>(v));
    }
  }

  Value parseValue(std::istream& is);
  std::string parseString(std::istream& is);

  Value parseObject(std::istream& is) {
    auto result = build_document();
    parseChar(is, '{');
    parseWhitespace(is);
    while (is.peek() != '}') {
      auto key = parseString(is);
      parseWhitespace(is);
      parseChar(is, ':');
      result = std::move(result) << key.c_str() << parseValue(is);
      if (is.peek() == ',') {
        is.get();
        parseWhitespace(is);
      }
    }
    parseChar(is, '}');
    return std::move(result) << ValueEnd{};
  }

  Value parseArray(std::istream& is) {
    auto result = build_array();
    parseChar(is, '[');
    parseWhitespace(is);
    while (is.peek() != ']') {
      result = std::move(result) << parseValue(is);
      if (is.peek() == ',') {
        is.get();
        parseWhitespace(is);
      }
    }
    parseChar(is, ']');
    return std::move(result) << ValueEnd{};
  }

  std::string parseString(std::istream& is) {
    auto result = std::string{};
    parseChar(is, '"');
    while (is.peek() != '"') {
      if (is.peek() != '\\') {
        result.push_back(is.get());
      } else {
        is.get();
        switch (is.get()) {
          case '"':
            result.push_back('"');
            break;
          case '\\':
            result.push_back('\\');
            break;
          case '/':
            result.push_back('/');
            break;
          case 'b':
            result.push_back('\b');
            break;
          case 'f':
            result.push_back('\f');
            break;
          case 'n':
            result.push_back('\n');
            break;
          case 'r':
            result.push_back('\r');
            break;
          case 't':
            result.push_back('\t');
            break;
          case 'u':
            break;
          default:
            throw std::invalid_argument("");
        }
      }
    }
    parseChar(is, '"');
    return result;
  }

  Value parseNumber(std::istream& is) {
    auto result = 0.0f;
    is >> result;
    return *(Array{} << result << ValueEnd{}).begin();
  }

  Value parseLiteral(std::istream& is, const char* literal, const Value& value) {
    while (*literal != '\0') {
      if (is.get() != *literal++) {
        throw std::invalid_argument("");
      }
    }
    return value;
  }

  Value parseValue(std::istream& is) {
    auto result = Value{};
    parseWhitespace(is);
    switch (is.peek()) {
      case '"':
        result = Value(*(Array{} << parseString(is) << ValueEnd{}).begin());
        break;
      case '{':
        result = parseObject(is);
        break;
      case '[':
        result = parseArray(is);
        break;
      case 't':
        result = parseLiteral(is, "true", *(Array{} << true << ValueEnd{}).begin());
        break;
      case 'f':
        result = parseLiteral(is, "false", *(Array{} << false << ValueEnd{}).begin());
        break;
      case 'n':
        result = parseLiteral(is, "null", *(Array{} << nullptr << ValueEnd{}).begin());
        break;
      default:
        result = parseNumber(is);
    }
    parseWhitespace(is);
    return result;
  }
}



std::istream& operator>>(std::istream& is, Value& value) {
  value = parseValue(is);
  return is;
}


static void quoted_string(std::ostream& os, const char* s) {
    if (s == nullptr) {
        os << "null";
    } else {
        os << '"';
        while (*s != '\0') {
            switch (*s) {
                case '"':
                    os << '\\' << '"';
                    break;
                case '\\':
                    os << '\\' << '\\';
                    break;
                case '/':
                    os << '\\' << '/';
                    break;
                case '\b':
                    os << '\\' << 'b';
                    break;
                case '\f':
                    os << '\\' << 'f';
                    break;
                case '\n':
                    os << '\\' << 'n';
                    break;
                case '\r':
                    os << '\\' << 'r';
                    break;
                case '\t':
                    os << '\\' << 't';
                    break;
                default:
                    os << *s;
                    break;
            }
            ++s;
        }
        os << '"';
    }
}

std::ostream& operator<<(std::ostream& os, const Value& value) {
    switch (value.type()) {
        case ValueType::_undefined:
        case ValueType::_binary:
        case ValueType::_null:
            os << "null";
            break;
        case ValueType::_string:
            quoted_string(os, value._c_str());
            break;
        case ValueType::_document: {
            os << '{';
            int index = 0;
            for (const auto &i : value) {
                if (index++ != 0)
                    os << ',';
                quoted_string(os, i.name());
                os << ':' << i;
            }
            os << '}';
            break;
        }
        case ValueType::_array: {
            os << '[';
            int index = 0;
            for (const auto &i : value) {
                if (index++ != 0)
                    os << ',';
                os << i;
            }
            os << ']';
            break;
        }
        case ValueType::_ObjectId:
            quoted_string(os, value._ObjectId().str().c_str());
            break;
        case ValueType::_boolean:
            os << (value._bool() ? "true" : "false");
            break;
        case ValueType::_int32:
            os << value._int32();
            break;
        case ValueType::_double:
            os << value._double();
            break;
    }
    return os;
}

