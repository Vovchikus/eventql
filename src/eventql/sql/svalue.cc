/**
 * Copyright (c) 2016 DeepCortex GmbH <legal@eventql.io>
 * Authors:
 *   - Paul Asmuth <paul@eventql.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdlib.h>
#include <string>
#include <ctime>
#include <stdint.h>
#include <eventql/util/inspect.h>
#include <eventql/util/human.h>
#include <eventql/sql/svalue.h>
#include <eventql/sql/format.h>
#include <eventql/sql/parser/token.h>

namespace csql {

SValue SValue::newNull() {
  return SValue();
}

SValue SValue::newString(const String& value) {
  return SValue(value);
}

SValue SValue::newString(const char* value) {
  return SValue(value);
}

SValue SValue::newInteger(IntegerType value) {
  return SValue(SValue::IntegerType(value));
}

SValue SValue::newInteger(const String& value) {
  return SValue(value).toInteger();
}

SValue SValue::newUInt64(uint64_t value) {
  SValue v;
  v.data_.type = SType::UINT64;
  v.data_.u.t_uint64 = value;
  return v;
}

SValue SValue::newInt64(int64_t value) {
  SValue v;
  v.data_.type = SType::INT64;
  v.data_.u.t_integer = value;
  return v;
}

SValue SValue::newFloat(FloatType value) {
  return SValue(value);
}

SValue SValue::newFloat(const String& value) {
  return SValue(value).toFloat();
}

SValue SValue::newBool(BoolType value) {
  return SValue(SValue::BoolType(value));
}

SValue SValue::newBool(const String& value) {
  return SValue(value).toBool();
}

SValue SValue::newTimestamp(TimeType value) {
  return SValue(value);
}

SValue SValue::newTimestamp(const String& value) {
  return SValue(value).toTimestamp();
}

SValue::SValue() {
  memset(&data_, 0, sizeof(data_));
  data_.type = SType::NIL;
}

SValue::~SValue() {
  switch (data_.type) {

    case SType::STRING:
      free(data_.u.t_string.ptr);
      break;

    default:
      break;

  }
}

SValue::SValue(const SValue::StringType& string_value) {
  data_.type = SType::STRING;
  data_.u.t_string.len = string_value.size();
  data_.u.t_string.ptr = static_cast<char *>(malloc(data_.u.t_string.len));

  if (data_.u.t_string.ptr == nullptr) {
    RAISE(kRuntimeError, "could not allocate SValue");
  }

  memcpy(
      data_.u.t_string.ptr,
      string_value.data(),
      data_.u.t_string.len);
}

SValue::SValue(
    char const* string_value) :
    SValue(std::string(string_value)) {}

SValue::SValue(SValue::IntegerType integer_value) {
  data_.type = SType::INT64;
  data_.u.t_integer = integer_value;
}

SValue::SValue(SValue::FloatType float_value) {
  data_.type = SType::FLOAT64;
  data_.u.t_float = float_value;
}

SValue::SValue(SValue::BoolType bool_value) {
  data_.type = SType::BOOL;
  data_.u.t_bool = bool_value;
}

SValue::SValue(SValue::TimeType time_value) {
  data_.type = SType::TIMESTAMP64;
  data_.u.t_timestamp = static_cast<uint64_t>(time_value);
}

SValue::SValue(const SValue& copy) {
  switch (copy.data_.type) {

    case SType::STRING:
      data_.type = SType::STRING;
      data_.u.t_string.len = copy.data_.u.t_string.len;
      data_.u.t_string.ptr = static_cast<char *>(malloc(data_.u.t_string.len));

      if (data_.u.t_string.ptr == nullptr) {
        RAISE(kRuntimeError, "could not allocate SValue");
      }

      memcpy(
          data_.u.t_string.ptr,
          copy.data_.u.t_string.ptr,
          data_.u.t_string.len);
      break;

    default:
      memcpy(&data_, &copy.data_, sizeof(data_));
      break;

  }

}

SValue& SValue::operator=(const SValue& copy) {
  if (data_.type == SType::STRING) {
    free(data_.u.t_string.ptr);
  }

  if (copy.data_.type == SType::STRING) {
    data_.type = SType::STRING;
    data_.u.t_string.len = copy.data_.u.t_string.len;
    data_.u.t_string.ptr = static_cast<char *>(malloc(data_.u.t_string.len));

    if (data_.u.t_string.ptr == nullptr) {
      RAISE(kRuntimeError, "could not allocate SValue");
    }

    memcpy(
        data_.u.t_string.ptr,
        copy.data_.u.t_string.ptr,
        data_.u.t_string.len);
  } else {
    memcpy(&data_, &copy.data_, sizeof(data_));
  }

  return *this;
}

bool SValue::operator==(const SValue& other) const {
  switch (data_.type) {

    case SType::INT64: {
      return getInteger() == other.getInteger();
    }

    case SType::TIMESTAMP64: {
      return getInteger() == other.getInteger();
    }

    case SType::FLOAT64: {
      return getFloat() == other.getFloat();
    }

    case SType::BOOL: {
      return getBool() == other.getBool();
    }

    case SType::STRING: {
      return memcmp(
          data_.u.t_string.ptr,
          other.data_.u.t_string.ptr,
          data_.u.t_string.len) == 0;
    }

    case SType::NIL: {
      return other.getInteger() == 0;
    }

  }
}

SType SValue::getType() const {
  return data_.type;
}

template <> SValue::BoolType SValue::getValue<SValue::BoolType>() const {
  return getBool();
}

template <> SValue::IntegerType SValue::getValue<SValue::IntegerType>() const {
  return getInteger();
}

template <> SValue::FloatType SValue::getValue<SValue::FloatType>() const {
  return getFloat();
}

template <> SValue::StringType SValue::getValue<SValue::StringType>() const {
  return getString();
}

template <> SValue::TimeType SValue::getValue<SValue::TimeType>() const {
  return getTimestamp();
}

// FIXPAUL: smarter type detection
template <> bool SValue::isConvertibleTo<SValue::BoolType>() const {
  return data_.type == SType::BOOL;
}

template <> bool SValue::isConvertibleTo<SValue::TimeType>() const {
  if (data_.type == SType::TIMESTAMP64) {
    return true;
  }

  return isConvertibleToNumeric();
}

SValue::IntegerType SValue::getInteger() const {
  switch (data_.type) {

    case SType::INT64:
      return data_.u.t_integer;

    case SType::TIMESTAMP64:
      return data_.u.t_timestamp;

    case SType::FLOAT64:
      return data_.u.t_float;

    case SType::BOOL:
      return data_.u.t_bool;

    case SType::NIL:
      return 0;

    case SType::STRING:
      try {
        return std::stoll(getString());
      } catch (std::exception e) {
        /* fallthrough */
      }

    default:
      RAISE(
          kTypeError,
          "can't convert %s '%s' to Integer",
          SValue::getTypeName(data_.type),
          getString().c_str());

  }

  return 0;
}

SValue SValue::toInteger() const {
  if (data_.type == SType::INT64) {
    return *this;
  } else {
    return SValue::newInteger(getInteger());
  }
}

SValue::FloatType SValue::getFloat() const {
  switch (data_.type) {

    case SType::INT64:
      return data_.u.t_integer;

    case SType::TIMESTAMP64:
      return data_.u.t_timestamp;

    case SType::FLOAT64:
      return data_.u.t_float;

    case SType::BOOL:
      return data_.u.t_bool;

    case SType::NIL:
      return 0;

    case SType::STRING:
      try {
        return std::stod(getString());
      } catch (std::exception e) {
        /* fallthrough */
      }

    default:
      RAISE(
          kTypeError,
          "can't convert %s '%s' to Float",
          SValue::getTypeName(data_.type),
          getString().c_str());

  }

  return 0;
}

SValue SValue::toFloat() const {
  if (data_.type == SType::FLOAT64) {
    return *this;
  } else {
    return SValue::newFloat(getFloat());
  }
}

SValue SValue::toBool() const {
  if (data_.type == SType::BOOL) {
    return *this;
  } else {
    return SValue::newBool(getBool());
  }
}

SValue::BoolType SValue::getBool() const {
  switch (data_.type) {

    case SType::INT64:
      return getInteger() > 0;

    case SType::FLOAT64:
      return getFloat() > 0;

    case SType::BOOL:
      return data_.u.t_bool;

    case SType::STRING:
      return true;

    case SType::NIL:
      return false;

    default:
      RAISEF(
         kTypeError,
          "can't convert $0 '$1' to Boolean",
          SValue::getTypeName(data_.type),
          getString());

  }
}

SValue::TimeType SValue::getTimestamp() const {
  if (isTimestamp()) {
    return data_.u.t_timestamp;
  }

  if (isConvertibleTo<TimeType>()) {
    return toTimestamp().getTimestamp();
  } else {
    RAISE(
       kTypeError,
        "can't convert %s '%s' to TIMESTAMP",
        SValue::getTypeName(data_.type),
        getString().c_str());
  }
}

std::string SValue::makeUniqueKey(SValue* arr, size_t len) {
  std::string key;

  for (int i = 0; i < len; ++i) {
    key.append(arr[i].getString());
    key.append("\x00");
  }

  return key;
}

SValue SValue::toString() const {
  if (data_.type == SType::STRING) {
    return *this;
  } else {
    return SValue::newString(getString());
  }
}


std::string SValue::getString() const {
  if (data_.type == SType::STRING) {
    return std::string(data_.u.t_string.ptr, data_.u.t_string.len);
  }

  char buf[512];
  const char* str;
  size_t len;

  switch (data_.type) {

    case SType::INT64: {
      len = snprintf(buf, sizeof(buf), "%" PRId64, getInteger());
      str = buf;
      break;
    }

    case SType::TIMESTAMP64: {
      return getTimestamp().toString("%Y-%m-%d %H:%M:%S");
    }

    case SType::FLOAT64: {
      len = snprintf(buf, sizeof(buf), "%f", getFloat());
      str = buf;
      break;
    }

    case SType::BOOL: {
      static const auto true_str = "true";
      static const auto false_str = "false";
      if (getBool()) {
        str = true_str;
        len = strlen(true_str);
      } else {
        str = false_str;
        len = strlen(false_str);
      }
      break;
    }

    case SType::STRING: {
      return getString();
    }

    case SType::NIL: {
      static const char undef_str[] = "NULL";
      str = undef_str;
      len = sizeof(undef_str) - 1;
    }

  }

  return std::string(str, len);
}

String SValue::toSQL() const {
  switch (data_.type) {

    case SType::INT64: {
      return getString();
    }

    case SType::TIMESTAMP64: {
      return StringUtil::toString(getInteger());
    }

    case SType::FLOAT64: {
      return getString();
    }

    case SType::BOOL: {
      return getString();
    }

    case SType::STRING: {
      auto str = sql_escape(getString());
      return StringUtil::format("\"$0\"", str);
    }

    case SType::NIL: {
      return "NULL";
    }

  }
}

const char* SValue::getTypeName(SType type) {
  switch (type) {
    case SType::STRING:
      return "String";
    case SType::FLOAT64:
      return "Float";
    case SType::INT64:
      return "Integer";
    case SType::BOOL:
      return "Boolean";
    case SType::TIMESTAMP64:
      return "Timestamp";
    case SType::NIL:
      return "NULL";
  }
}

const char* SValue::getTypeName() const {
  return SValue::getTypeName(data_.type);
}

template <> bool SValue::isConvertibleTo<SValue::IntegerType>() const {
  switch (data_.type) {
    case SType::INT64:
    case SType::TIMESTAMP64:
      return true;
    default:
      break;
  }

  auto str = getString();
  const char* cur = str.c_str();
  const char* end = cur + str.size();

  if (*cur == '-') {
    ++cur;
  }

  if (cur == end) {
    return false;
  }

  for (; cur < end; ++cur) {
    if (*cur < '0' || *cur > '9') {
      return false;
    }
  }

  return true;
}

template <> bool SValue::isConvertibleTo<SValue::FloatType>() const {
  switch (data_.type) {
    case SType::FLOAT64:
    case SType::INT64:
    case SType::TIMESTAMP64:
      return true;
    default:
      break;
  }

  auto str = getString();
  bool dot = false;
  const char* c = str.c_str();

  if (*c == '-') {
    ++c;
  }

  for (; *c != 0; ++c) {
    if (*c >= '0' && *c <= '9') {
      continue;
    }

    if (*c == '.' || *c == ',') {
      if (dot) {
        return false;
      } else {
        dot = true;
      }
      continue;
    }

    return false;
  }

  return true;
}

template <> bool SValue::isConvertibleTo<std::string>() const {
  return true;
}

SValue SValue::toTimestamp() const {
  if (isTimestamp()) {
    return *this;
  }

  if (isConvertibleToNumeric()) {
    return SValue(SValue::TimeType(toNumeric().getFloat()));
  }

  RAISE(
      kTypeError,
      "can't convert %s '%s' to TIMESTAMP",
      SValue::getTypeName(data_.type),
      getString().c_str());
}

SValue SValue::toNumeric() const {
  if (isNumeric()) {
    return *this;
  }

  if (isConvertibleTo<SValue::IntegerType>()) {
    return SValue(SValue::IntegerType(getInteger()));
  }

  if (isConvertibleTo<SValue::FloatType>()) {
    return SValue(SValue::FloatType(getFloat()));
  }

  RAISE(
      kTypeError,
      "can't convert %s '%s' to NUMERIC",
      SValue::getTypeName(data_.type),
      getString().c_str());
}

bool SValue::isString() const {
  return data_.type == SType::STRING;
}

bool SValue::isFloat() const {
  return data_.type == SType::FLOAT64;
}

bool SValue::isInteger() const {
  return data_.type == SType::INT64;
}

bool SValue::isBool() const {
  return data_.type == SType::BOOL;
}

bool SValue::isTimestamp() const {
  return data_.type == SType::TIMESTAMP64;
}

bool SValue::isNumeric() const {
  switch (data_.type) {
    case SType::FLOAT64:
    case SType::INT64:
      return true;
    default:
      return false;
  }
}

bool SValue::isConvertibleToNumeric() const {
  if (isConvertibleTo<SValue::IntegerType>() ||
      isConvertibleTo<SValue::FloatType>() ||
      isTimestamp()) {
    return true;
  } else {
    return false;
  }
}

bool SValue::isConvertibleToBool() const {
  switch (data_.type) {
    case SType::STRING:
    case SType::FLOAT64:
    case SType::INT64:
    case SType::BOOL:
    case SType::NIL:
      return true;
    case SType::TIMESTAMP64:
      return false;
  }
}

void SValue::encode(OutputStream* os) const {
  os->appendUInt8((uint8_t) data_.type);

  switch (data_.type) {
    case SType::STRING:
      os->appendLenencString(data_.u.t_string.ptr, data_.u.t_string.len);
      return;
    case SType::FLOAT64:
      os->appendDouble(data_.u.t_float);
      return;
    case SType::INT64:
      os->appendUInt64(data_.u.t_integer);
      return;
    case SType::UINT64:
      os->appendUInt64(data_.u.t_integer);
      return;
    case SType::BOOL:
      os->appendUInt8(data_.u.t_bool ? 1 : 0);
      return;
    case SType::TIMESTAMP64:
      os->appendUInt64(data_.u.t_timestamp);
      return;
    case SType::NIL:
      return;
  }
}

void SValue::decode(InputStream* is) {
  SType type = (SType) is->readUInt8();

  switch (type) {
    case SType::STRING:
      *this = SValue(is->readLenencString());
      return;
    case SType::FLOAT64:
      *this = SValue(SValue::FloatType(is->readDouble()));
      return;
    case SType::INT64:
      *this = SValue::newInt64(is->readUInt64());
      return;
    case SType::UINT64:
      *this = SValue::newUInt64(is->readUInt64());
      return;
    case SType::BOOL:
      *this = SValue(SValue::BoolType(is->readUInt8() == 1));
      return;
    case SType::TIMESTAMP64: {
      auto v = is->readUInt64();
      *this = SValue(SValue::TimeType(v));
      return;
    }
    case SType::NIL:
      *this = SValue();
      return;
  }
}

String sql_escape(const String& orig_str) {
  auto str = orig_str;
  StringUtil::replaceAll(&str, "\\", "\\\\");
  StringUtil::replaceAll(&str, "'", "\\'");
  StringUtil::replaceAll(&str, "\"", "\\\"");
  return str;
}

template <> bool SValue::isOfType<SValue::StringType>() const {
  return isString();
}

template <> bool SValue::isOfType<SValue::FloatType>() const {
  return isFloat();
}

template <> bool SValue::isOfType<SValue::IntegerType>() const {
  return isInteger();
}

template <> bool SValue::isOfType<SValue::BoolType>() const {
  return isBool();
}

template <> bool SValue::isOfType<SValue::TimeType>() const {
  return isTimestamp();
}

}

template <>
std::string inspect<csql::SType>(
    const csql::SType& type) {
  return csql::SValue::getTypeName(type);
}

template <>
std::string inspect<csql::SValue>(
    const csql::SValue& sval) {
  return sval.getString();
}

namespace std {

size_t hash<csql::SValue>::operator()(const csql::SValue& sval) const {
  return hash<std::string>()(sval.getString()); // FIXPAUL
}

}
